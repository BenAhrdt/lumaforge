const http = require('node:http');
const fs = require('node:fs/promises');
const path = require('node:path');
const { WebSocketServer, WebSocket } = require('ws');
const { FileStorage, validateLayout, validateZones, validateScenes, validateAutomations } = require('./storage');
const { CoreEngine } = require('./engine');

const root = path.resolve(__dirname, '..'); const webRoot = path.join(root, 'web');
const port = Number(process.env.PORT || 80); const storage = new FileStorage(process.env.CONFIG_DIR || path.join(root, 'config'));
const defaults = {
  device: { name:'LumaForge Simulator', ledType:'WS2812B', ledCount:30, gpio:4, channels:'RGB', colorOrder:'RGB', maxBrightness:1, maxCurrentMa:3000, supplyVoltage:5, outputs:[{id:'main',gpio:4}] },
  layout: { strips:[] }, zones: [], scenes: [], automations: []
};
let project, preview = null, activeSceneId = null, epoch = Date.now();
let previousCpuUsage=process.cpuUsage(),previousCpuAt=process.hrtime.bigint();
const ledCount = () => project.layout.strips.reduce((sum, strip) => sum + strip.ledCount, 0);
const allLeds = () => Array.from({length:ledCount()},(_,i)=>i);
const zoneLeds = id => id === 'all' ? allLeds() : (project.zones.find(z=>z.id===id)?.leds || []);
const animations = () => {
  const scene = project.scenes.find(s=>s.id===activeSceneId); const list = (scene?.animations || []).map((a,i)=>({...a,duration:(a.loop||scene.loop)?0:a.duration,id:a.id||`animation-${i}`,leds:zoneLeds(a.zoneId),brightness:a.brightness??1,priority:a.priority??0,direction:a.direction||'forward'}));
  if (preview) list.push({id:'preview',effect:preview.effect||'solid',leds:preview.selection,color:preview.color,brightness:preview.brightness,speed:preview.speed||1,start:0,duration:86400,priority:1000,direction:preview.direction||'forward'});
  return list;
};
const engine = new CoreEngine(path.join(root, 'build/lumaforge-engine'));
let engineSync = Promise.resolve();
function syncEngine(){const count=ledCount(),configuration=structuredClone(animations());engineSync=engineSync.then(async()=>{epoch=Date.now();await engine.configure(count,configuration);});return engineSync;}
function migrateLayout(layout){const byOutput=new Map();for(const strip of layout.strips){strip.output=strip.output||'main';const sections=byOutput.get(strip.output)||[];strip.order=Number.isInteger(strip.order)?strip.order:sections.length;strip.gpio=Number.isInteger(strip.gpio)?strip.gpio:(project.device.outputs?.find(o=>o.id===strip.output)?.gpio??project.device.gpio??4);strip.colorOrder=strip.colorOrder||project.device.colorOrder||'RGB';strip.spacingCm=Number(strip.spacingCm)||10;strip.physicalLengthCm=Number(strip.physicalLengthCm)||Math.hypot(strip.x2-strip.x1,strip.y2-strip.y1)/24*10;strip.sizing=strip.sizing||'length';sections.push(strip);byOutput.set(strip.output,sections);}for(const sections of byOutput.values()){sections.sort((a,b)=>a.order-b.order);let index=0;for(let i=0;i<sections.length;i++){const strip=sections[i];if(i){const previous=sections[i-1],dx=previous.x2-strip.x1,dy=previous.y2-strip.y1;strip.x1+=dx;strip.y1+=dy;strip.x2+=dx;strip.y2+=dy;}strip.startIndex=index;index+=strip.ledCount;}}return layout;}
async function load(){project={device:await storage.read('device',defaults.device),layout:await storage.read('layout',defaults.layout),zones:await storage.read('zones',defaults.zones),scenes:await storage.read('scenes',defaults.scenes),automations:await storage.read('automations',defaults.automations)};migrateLayout(project.layout);validateLayout(project.layout);validateZones(project.zones,ledCount());validateScenes(project.scenes,project.zones);validateAutomations(project.automations,project.scenes);await storage.write('layout',project.layout);activeSceneId=null;await syncEngine();}
function json(res,status,data){const body=JSON.stringify(data);res.writeHead(status,{'content-type':'application/json','content-length':Buffer.byteLength(body)});res.end(body);}
async function body(req){let data='';for await(const chunk of req){data+=chunk;if(data.length>1e6)throw new Error('request too large');}return JSON.parse(data||'{}');}
const api = async(req,res,url)=>{
  if(req.method==='GET'&&url.pathname==='/api/v1/status'){const now=process.hrtime.bigint(),usage=process.cpuUsage(previousCpuUsage),elapsed=Number(now-previousCpuAt)/1000;previousCpuUsage=process.cpuUsage();previousCpuAt=now;const memory=process.memoryUsage();return json(res,200,{version:'0.1.0-alpha.1',cpuPercent:Math.min(100,(usage.user+usage.system)/Math.max(1,elapsed)*100),memoryUsedBytes:memory.heapUsed,memoryTotalBytes:memory.heapTotal});}
  if(req.method==='GET'&&url.pathname==='/api/config')return json(res,200,{...project.device,apiVersion:1});
  const routes={layout:'layout',zones:'zones',scenes:'scenes',automations:'automations'};const key=routes[url.pathname.split('/')[2]];
  if(key&&req.method==='GET')return json(res,200,project[key]);
  if(key&&req.method==='PUT'){const value=await body(req);if(key==='layout')validateLayout(value);if(key==='zones')validateZones(value,ledCount());if(key==='scenes')validateScenes(value,project.zones);if(key==='automations')validateAutomations(value,project.scenes);project[key]=value;if(key==='scenes'){activeSceneId=null;preview=null;}await storage.write(key,value);await syncEngine();broadcast({type:`${key}.updated`,payload:value});return json(res,200,value);}
  if(req.method==='GET'&&url.pathname==='/api/project')return json(res,200,project);
  if(req.method==='POST'&&url.pathname==='/api/project/import'){const value=await body(req);validateLayout(value.layout);const count=value.layout.strips.reduce((n,s)=>n+s.ledCount,0);validateZones(value.zones,count);validateScenes(value.scenes,value.zones);validateAutomations(value.automations||[],value.scenes);project={device:value.device||defaults.device,layout:value.layout,zones:value.zones,scenes:value.scenes,automations:value.automations||[]};await Promise.all(Object.entries(project).map(([k,v])=>storage.write(k,v)));await syncEngine();return json(res,200,project);}
  json(res,404,{error:'Not found'});
};
const mime={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.svg':'image/svg+xml'};
const server=http.createServer(async(req,res)=>{try{const url=new URL(req.url,'http://localhost');if(url.pathname.startsWith('/api/'))return await api(req,res,url);const relative=url.pathname==='/'?'index.html':url.pathname.slice(1);const target=path.resolve(webRoot,relative);if(!target.startsWith(webRoot))return json(res,403,{error:'Forbidden'});const data=await fs.readFile(target);res.writeHead(200,{'content-type':mime[path.extname(target)]||'application/octet-stream'});res.end(data);}catch(error){if(error.code==='ENOENT')return json(res,404,{error:'Not found'});console.error(error);json(res,400,{error:error.message});}});
const wss=new WebSocketServer({server,path:'/ws'});function broadcast(message){const data=JSON.stringify(message);for(const client of wss.clients)if(client.readyState===WebSocket.OPEN)client.send(data);}
wss.on('connection',ws=>{ws.send(JSON.stringify({type:'hello',apiVersion:1}));ws.on('message',async raw=>{try{const msg=JSON.parse(raw);if(msg.type==='preview.set'){if(!Array.isArray(msg.selection)||msg.selection.some(i=>!Number.isInteger(i)||i<0||i>=ledCount()))throw new Error('invalid selection');if(!/^#[0-9a-f]{6}$/i.test(msg.color))throw new Error('invalid color');preview={selection:msg.selection,color:msg.color,brightness:Math.max(0,Math.min(1,Number(msg.brightness))),effect:msg.effect,speed:Math.max(.05,Number(msg.speed)||1),direction:msg.direction};await syncEngine();}else if(msg.type==='preview.cancel'){preview=null;await syncEngine();}else if(msg.type==='preview.apply'){preview=null;await syncEngine();}else if(msg.type==='scene.play'){if(!project.scenes.some(s=>s.id===msg.sceneId))throw new Error('unknown scene');activeSceneId=msg.sceneId;preview=null;await syncEngine();}else if(msg.type==='scene.stop'){activeSceneId=null;preview=null;await syncEngine();}else throw new Error('unknown message type');}catch(error){ws.send(JSON.stringify({type:'error',message:error.message}));}});});
let ticking=false;setInterval(async()=>{if(ticking)return;ticking=true;try{broadcast({type:'frame',pixels:await engine.frame((Date.now()-epoch)/1000)});}catch(error){console.error(error);}finally{ticking=false;}},33);
load().then(()=>server.listen(port,'0.0.0.0',()=>console.log(`LumaForge: http://0.0.0.0:${port}`))).catch(error=>{console.error(error);engine.close();process.exit(1);});
for(const signal of ['SIGINT','SIGTERM'])process.on(signal,()=>{engine.close();server.close(()=>process.exit(0));});
