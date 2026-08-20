const fs = require('node:fs/promises');
const path = require('node:path');

class FileStorage {
  constructor(directory) { this.directory = directory; }
  async read(name, fallback) {
    try { return JSON.parse(await fs.readFile(path.join(this.directory, `${name}.json`), 'utf8')); }
    catch (error) { if (error.code === 'ENOENT') return structuredClone(fallback); throw error; }
  }
  async write(name, data) {
    await fs.mkdir(this.directory, { recursive: true });
    const target = path.join(this.directory, `${name}.json`);
    const temporary = `${target}.tmp`;
    await fs.writeFile(temporary, `${JSON.stringify(data, null, 2)}\n`);
    await fs.rename(temporary, target);
    return data;
  }
}

function validateLayout(layout) {
  const ledOutputGpios = new Set([4,5,13,14]);
  const colorOrders = new Set(['RGB','GRB','BRG','RBG','GBR','BGR']);
  if (!layout || !Array.isArray(layout.strips)) throw new Error('layout.strips must be an array');
  const ids = new Set();
  for (const strip of layout.strips) {
    if (!strip.id || ids.has(strip.id)) throw new Error('strip IDs must be non-empty and unique');
    ids.add(strip.id);
    const cable=strip.type==='cable';
    if (!Number.isInteger(strip.ledCount) || (cable ? strip.ledCount !== 0 : strip.ledCount < 1) || strip.ledCount > 4096) throw new Error(cable?'cable sections cannot contain LEDs':'ledCount must be 1..4096');
    for (const key of ['x1','y1','x2','y2']) if (!Number.isFinite(strip[key])) throw new Error(`${key} must be finite`);
    if (strip.gpio !== undefined && !ledOutputGpios.has(strip.gpio)) throw new Error('GPIO is not supported for LED output');
    if (strip.colorOrder !== undefined && !colorOrders.has(strip.colorOrder)) throw new Error('invalid strip color order');
    if (strip.spacingCm !== undefined && (!Number.isFinite(strip.spacingCm) || strip.spacingCm <= 0)) throw new Error('LED spacing must be positive');
  }
  const outputs = new Map();
  for (const strip of layout.strips) { const key=strip.output||'main',list=outputs.get(key)||[];list.push(strip);outputs.set(key,list); }
  for (const sections of outputs.values()) { sections.sort((a,b)=>(a.order??0)-(b.order??0));const gpios=new Set(sections.map(s=>s.gpio).filter(Number.isInteger)),orders=new Set(sections.map(s=>s.colorOrder).filter(Boolean));if(gpios.size>1)throw new Error('one LED run must use one GPIO');if(orders.size>1)throw new Error('one LED run must use one color order');for(let i=1;i<sections.length;i++)if(Math.hypot(sections[i].x1-sections[i-1].x2,sections[i].y1-sections[i-1].y2)>.01)throw new Error('sections in one LED run must be connected'); }
  const gpioOwners = new Map();
  for (const [output,sections] of outputs) { const gpio=sections.find(section=>Number.isInteger(section.gpio))?.gpio;if(gpioOwners.has(gpio)&&gpioOwners.get(gpio)!==output)throw new Error('GPIO may only be assigned to one LED run');if(Number.isInteger(gpio))gpioOwners.set(gpio,output); }
  return layout;
}
function validateZones(zones, ledCount) {
  if (!Array.isArray(zones)) throw new Error('zones must be an array');
  const ids = new Set();
  for (const zone of zones) {
    if (!zone.id || !String(zone.name || '').trim() || ids.has(zone.id)) throw new Error('zone IDs and names must be unique and non-empty');
    ids.add(zone.id);
    if (!Array.isArray(zone.leds) || zone.leds.some(i => !Number.isInteger(i) || i < 0 || i >= ledCount)) throw new Error('zone contains an invalid LED');
  }
  return zones;
}
function validateScenes(scenes, zones) {
  if (!Array.isArray(scenes)) throw new Error('scenes must be an array');
  const zoneIds = new Set(zones.map(z => z.id)); const sceneIds = new Set();
  for (const scene of scenes) {
    if (!scene.id || !String(scene.name || '').trim() || sceneIds.has(scene.id)) throw new Error('scene IDs and names must be unique and non-empty');
    sceneIds.add(scene.id);
    if(scene.loop!==undefined&&typeof scene.loop!=='boolean')throw new Error('scene.loop must be boolean');
    if (!Array.isArray(scene.animations)) throw new Error('scene.animations must be an array');
    for (const animation of scene.animations) {
      if (!zoneIds.has(animation.zoneId) && animation.zoneId !== 'all') throw new Error(`unknown zone: ${animation.zoneId}`);
      if (!/^#[0-9a-f]{6}$/i.test(animation.color || '')) throw new Error('invalid animation color');
      if(animation.colorMode!==undefined&&!['solid','rainbow'].includes(animation.colorMode))throw new Error('invalid animation color mode');
      if (![animation.start, animation.duration, animation.speed].every(Number.isFinite) || animation.start < 0 || animation.duration <= 0 || animation.speed <= 0) throw new Error('invalid animation timing');
      if(animation.width!==undefined&&(!Number.isFinite(animation.width)||animation.width<=0||animation.width>1))throw new Error('invalid animation width');
      if(animation.loop!==undefined&&typeof animation.loop!=='boolean')throw new Error('animation.loop must be boolean');
    }
  }
  return scenes;
}
function validateAutomations(automations, scenes) {
  if (!Array.isArray(automations)) throw new Error('automations must be an array');
  const sceneIds=new Set(scenes.map(scene=>scene.id)),ids=new Set();
  for(const automation of automations){
    if(!automation.id||!String(automation.name||'').trim()||ids.has(automation.id))throw new Error('automation IDs and names must be unique and non-empty');
    ids.add(automation.id);
    const steps=automation.steps??(automation.sceneId?[{sceneId:automation.sceneId,advance:'manual'}]:null);
    if(!Array.isArray(steps)||!steps.length)throw new Error('automation.steps must be a non-empty array');
    for(const step of steps){
      if(!sceneIds.has(step.sceneId))throw new Error(`unknown scene: ${step.sceneId}`);
      if(!['scene_finished','after_delay','manual'].includes(step.advance))throw new Error('invalid automation step advance mode');
      if(step.advance==='after_delay'&&(!Number.isFinite(step.durationSeconds)||step.durationSeconds<=0))throw new Error('automation step durationSeconds must be positive');
    }
    if(!['manual','time'].includes(automation.trigger))throw new Error('invalid automation trigger');
    if(automation.trigger==='time'&&!/^([01]\d|2[0-3]):[0-5]\d$/.test(automation.time||''))throw new Error('invalid automation time');
    if(typeof automation.enabled!=='boolean')throw new Error('automation.enabled must be boolean');
  }
  return automations;
}
module.exports = { FileStorage, validateLayout, validateZones, validateScenes, validateAutomations };
