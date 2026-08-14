const { spawn } = require('node:child_process');
const readline = require('node:readline');

class CoreEngine {
  constructor(binary) {
    this.child = spawn(binary, [], { stdio: ['pipe', 'pipe', 'inherit'] });
    this.pending = [];
    readline.createInterface({ input: this.child.stdout }).on('line', line => this.pending.shift()?.(line));
    this.child.on('exit', code => { const error = `Core engine exited (${code})`; while (this.pending.length) this.pending.shift()(error); });
  }
  command(value) { return new Promise((resolve, reject) => {
    this.pending.push(line => line.startsWith('ERROR|') ? reject(new Error(line.slice(6))) : resolve(line));
    this.child.stdin.write(`${value}\n`);
  }); }
  async configure(count, animations) {
    await this.command(`COUNT|${count}`); await this.command('CLEAR');
    for (const a of animations) await this.command(['ANIM', a.id, a.effect, a.leds.join(','), a.color,
      a.brightness, a.speed, a.start, a.duration, a.priority, a.direction || (a.reverse ? 'reverse' : 'forward'), a.width ?? .15,
      a.colorMode || 'solid'].join('|'));
  }
  async frame(seconds) { const line = await this.command(`TICK|${seconds}`); return JSON.parse(line.slice(6)); }
  close() { this.child.kill(); }
}
module.exports = { CoreEngine };
