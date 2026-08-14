const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { FileStorage, validateLayout, validateZones, validateScenes, validateAutomations } = require('./storage');

test('FileStorage round-trips JSON atomically', async () => {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'lumaforge-'));
  const storage = new FileStorage(directory); const value = { strips:[{ id:'one', ledCount:2 }] };
  await storage.write('layout', value); assert.deepEqual(await storage.read('layout', {}), value);
});
test('layout rejects duplicate strip IDs', () => assert.throws(() => validateLayout({strips:[
  {id:'x',ledCount:1,x1:0,y1:0,x2:1,y2:1},{id:'x',ledCount:1,x1:0,y1:0,x2:1,y2:1}]}), /unique/));
test('layout only accepts suitable, uniquely assigned LED GPIOs', () => {
  const strip=(id,output,gpio,y=0)=>({id,output,gpio,ledCount:1,x1:0,y1:y,x2:1,y2:y});
  assert.doesNotThrow(()=>validateLayout({strips:[strip('a','one',4),strip('b','two',5,1)]}));
  assert.throws(()=>validateLayout({strips:[strip('a','one',2)]}),/not supported/);
  assert.throws(()=>validateLayout({strips:[strip('a','one',4),strip('b','two',4,1)]}),/one LED run/);
});
test('zone rejects out-of-range LEDs', () => assert.throws(() => validateZones([{id:'x',name:'X',leds:[3]}],3), /invalid LED/));
test('scene validates zone references and timing', () => {
  assert.doesNotThrow(()=>validateScenes([{id:'s',name:'S',animations:[{zoneId:'z',color:'#00aeef',start:0,duration:1,speed:1}]}],[{id:'z'}]));
  assert.throws(()=>validateScenes([{id:'s',name:'S',animations:[{zoneId:'missing',color:'#00aeef',start:0,duration:1,speed:1}]}],[]),/unknown zone/);
});
test('automation validates trigger and scene reference',()=>{
  assert.doesNotThrow(()=>validateAutomations([{id:'a',name:'Evening',sceneId:'s',trigger:'time',time:'18:30',enabled:true}],[{id:'s'}]));
  assert.throws(()=>validateAutomations([{id:'a',name:'Bad',sceneId:'missing',trigger:'manual',enabled:true}],[]),/unknown scene/);
});
