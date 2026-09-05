const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawn, spawnSync } = require('node:child_process');

const base = fs.realpathSync(os.tmpdir());
const root = fs.mkdtempSync(path.join(base, 'easykey-module-test-'));
const quote = value => "'" + value.replaceAll('\\', '/').replaceAll("'", "'\"'\"'") + "'";
const installScript = fs.readFileSync('module/customize.sh', 'utf8');
const reloadScript = fs.readFileSync('module/reload.sh', 'utf8');
assert(reloadScript.startsWith('#!/data/adb/ksu/bin/busybox sh\n'));
assert(fs.readFileSync('module/service.sh', 'utf8').includes('/data/adb/ksu/bin/busybox sh "$MODDIR/reload.sh"'));
const read = file => fs.readFileSync(file, 'utf8');
const stage = name => {
  const destination = path.join(root, name);
  fs.cpSync('module', destination, { recursive: true });
  return destination;
};
function install(old, destination, failure = false) {
  const prefix = `getprop() { printf plk110; }
getevent() { printf 'EV_KEY KEY_VOLUMEUP DOWN\n'; }
ui_print() { printf '%s\n' "$*"; }
abort() { printf '%s\n' "$*" >&2; exit 42; }
MODPATH=${quote(destination)}
${failure ? 'cp() { return 1; }' : ''}
`;
  return spawnSync('sh', ['-c', prefix + installScript.replace('OLDMOD=/data/adb/modules/Easy_Key', 'OLDMOD=' + quote(old))], { encoding: 'utf8', windowsHide: true, timeout: 10000 });
}
function reload(destination, mode) {
  const groups = ['/acct', '/dev/cg2_bpf', '/sys/fs/cgroup', '/dev/memcg/apps'];
  let script = reloadScript.replace('/data/adb/ksu/bin/busybox flock', 'flock');
  for (const group of groups) {
    const directory = path.join(destination, group);
    fs.mkdirSync(directory, { recursive: true });
    if (mode === 'cgroup-write-fails' && group === '/dev/memcg/apps')
      fs.mkdirSync(path.join(directory, 'cgroup.procs'));
    else if (mode !== 'no-cgroups')
      fs.writeFileSync(path.join(directory, 'cgroup.procs'), '');
    script = script.replaceAll(group, directory.replaceAll('\\', '/'));
  }
  const membership = path.join(destination, 'cgroup');
  const memberships = {
    'cgroup-v2-stuck': '0::/uid_10234/pid_1234\n',
    'cgroup-v1-stuck': '2:memory:/apps/com.example.manager\n',
    'cgroup-nested-stuck': '0::/apps/uid_10234/pid_1234\n'
  };
  if (mode !== 'cgroup-read-fails')
    fs.writeFileSync(membership, memberships[mode] || '0::/\n1:cpuacct:/\n2:memory:/apps\n');
  script = script.replace('/proc/$$/cgroup', membership.replaceAll('\\', '/'));
  const prefix = `TRACE=${quote(path.join(destination, 'trace'))}
RUNNING=${quote(path.join(destination, 'running'))}
pidof() { [ -f "$RUNNING" ] || return 1; printf 4242; }
kill() {
    if [ "$1" = -0 ]; then return ${mode === 'start-fails' ? '1' : '0'}; fi
    ${mode === 'no-cgroups' ? '' : groups.map(group => `[ "$(cat ${quote(path.join(destination, group, 'cgroup.procs'))})" = "$$" ] || exit 91`).join('\n    ')}
    ${mode === 'stop-fails' ? 'return 1' : 'rm -f "$RUNNING"; printf "stop\\n" >> "$TRACE"'}
}
nohup() {
    [ "$1" = setsid ] && [ "$2" = "$MODDIR/EasyKey" ] && [ "$#" = 2 ] || return 92
    if read -r input; then return 93; fi
    printf 'start\n' >> "$TRACE"
}
sleep() { command sleep 0.01; }
`;
  return spawnSync('sh', ['-c', prefix + script, path.join(destination, 'reload.sh').replaceAll('\\', '/')], { input: 'webui stdin\n', encoding: 'utf8', windowsHide: true, timeout: 10000 });
}
async function testReloadLock() {
  const destination = stage('lock');
  const released = path.join(destination, 'release');
  fs.writeFileSync(path.join(destination, 'cgroup'), '0::/\n');
  let script = reloadScript.replace('/data/adb/ksu/bin/busybox flock', 'flock');
  for (const group of ['/acct', '/dev/cg2_bpf', '/sys/fs/cgroup', '/dev/memcg/apps'])
    script = script.replaceAll(group, path.join(destination, group).replaceAll('\\', '/'));
  script = script.replace('/proc/$$/cgroup', path.join(destination, 'cgroup').replaceAll('\\', '/'));
  const prefix = `chmod() {
    printf 'locked\n'
    while [ ! -f ${quote(released)} ]; do command sleep 0.01; done
}
pidof() { return 1; }
nohup() { printf 'start\n' >> ${quote(path.join(destination, 'trace'))}; command sleep 2; }
`;
  let unlock;
  const locked = new Promise(resolve => { unlock = resolve; });
  const first = spawn('sh', ['-c', prefix + script, path.join(destination, 'reload.sh').replaceAll('\\', '/')], { windowsHide: true, timeout: 5000 });
  const completed = new Promise((resolve, reject) => {
    let stdout = '', stderr = '';
    first.stdout.on('data', data => {
      stdout += data;
      if (stdout.includes('locked\n')) unlock(true);
    });
    first.stderr.on('data', data => { stderr += data; });
    first.on('error', reject);
    first.on('close', status => { unlock(false); resolve({ status, stdout, stderr }); });
  });
  try {
    assert.equal(await locked, true, 'First reload did not acquire the lock');
    const second = spawnSync('sh', ['-c', prefix + script, path.join(destination, 'reload.sh').replaceAll('\\', '/')], { encoding: 'utf8', windowsHide: true, timeout: 3000 });
    assert.equal(second.status, 1, second.stderr || String(second.error));
    assert(second.stderr.includes('重载锁'));
    assert.equal(second.stdout, '');
  } finally {
    fs.writeFileSync(released, '');
    await completed;
  }
  const result = await completed;
  assert.equal(result.status, 0, result.stderr);
  assert(result.stdout.includes('easykey_reload_ok'));
  assert.equal(read(path.join(destination, 'trace')), 'start\n');
  const available = spawnSync('flock', ['-n', path.join(destination, '.reload.lock'), '-c', 'true'], { encoding: 'utf8', windowsHide: true });
  assert.equal(available.status, 0, 'Backend inherited the reload lock: ' + available.stderr);
}

async function test() {
  try {
    for (const file of ['module/customize.sh', 'module/service.sh', 'module/reload.sh', 'module/ind/torch.sh']) {
      const checked = spawnSync('sh', ['-n', file], { encoding: 'utf8', windowsHide: true });
      assert.equal(checked.status, 0, checked.stderr || String(checked.error));
    }
    const old = path.join(root, 'old');
    fs.mkdirSync(path.join(old, 'ind', 'nested'), { recursive: true });
    fs.mkdirSync(path.join(old, 'backup', 'older', 'ind'), { recursive: true });
    fs.writeFileSync(path.join(old, 'config.ini'), 'click=echo user\nlong=\ndouble=');
    fs.writeFileSync(path.join(old, 'repo.json'), '[]');
    fs.writeFileSync(path.join(old, 'ind', 'torch.sh'), 'old custom torch');
    fs.writeFileSync(path.join(old, 'ind', 'my script.sh'), 'custom script');
    fs.writeFileSync(path.join(old, 'ind', 'nested', 'helper.sh'), 'nested helper');
    fs.writeFileSync(path.join(old, 'backup', 'older', 'ind', 'torch.sh'), 'earlier backup');
    fs.writeFileSync(path.join(old, 'action.sh'), 'old action');
    const fresh = stage('fresh');
    let result = install(path.join(root, 'absent'), fresh);
    assert.equal(result.status, 0, result.stderr || String(result.error));
    assert.equal(read(path.join(fresh, 'config.ini')), read('module/config.ini'));
    const next = stage('next');
    result = install(old, next);
    assert.equal(result.status, 0, result.stderr || String(result.error));
    assert.equal(read(path.join(next, 'config.ini')), read(path.join(old, 'config.ini')));
    assert.equal(read(path.join(next, 'repo.json')), '[]');
    assert.equal(read(path.join(next, 'ind', 'torch.sh')), read('module/ind/torch.sh'));
    assert.equal(read(path.join(next, 'ind', 'my script.sh')), 'custom script');
    assert.equal(read(path.join(next, 'ind', 'nested', 'helper.sh')), 'nested helper');
    assert.equal(read(path.join(old, 'action.sh')), 'old action');
    const backups = fs.readdirSync(path.join(next, 'backup'));
    const migrated = backups.find(name => name.startsWith('upgrade.'));
    assert.equal(read(path.join(next, 'backup', migrated, 'ind', 'torch.sh')), 'old custom torch');
    assert.equal(read(path.join(next, 'backup', 'older', 'ind', 'torch.sh')), 'earlier backup');
    const third = stage('third');
    result = install(next, third);
    assert.equal(result.status, 0, result.stderr || String(result.error));
    assert.equal(fs.readdirSync(path.join(third, 'backup')).length, backups.length + 1);
    assert.equal(read(path.join(third, 'backup', migrated, 'ind', 'torch.sh')), 'old custom torch');
    const failed = stage('failed');
    result = install(old, failed, true);
    assert.equal(result.status, 42);
    assert(result.stderr.includes('无法保留 config.ini'));
    assert(!result.stdout.includes('已保留 config.ini'));
    assert.equal(read(path.join(old, 'ind', 'torch.sh')), 'old custom torch');
    for (const mode of ['normal', 'no-cgroups', 'stop-fails', 'start-fails', 'cgroup-write-fails', 'cgroup-read-fails', 'cgroup-v2-stuck', 'cgroup-v1-stuck', 'cgroup-nested-stuck']) {
      const destination = stage(mode);
      fs.writeFileSync(path.join(destination, 'running'), 'old');
      result = reload(destination, mode);
      const available = spawnSync('flock', ['-n', path.join(destination, '.reload.lock'), '-c', 'true'], { encoding: 'utf8', windowsHide: true });
      assert.equal(available.status, 0, 'Reload did not release its lock: ' + available.stderr);
      const success = mode === 'normal' || mode === 'no-cgroups';
      assert.equal(result.status, success ? 0 : 1, result.stderr || String(result.error));
      if (success) {
        assert(result.stdout.includes('easykey_reload_ok'));
        assert.equal(read(path.join(destination, 'trace')), 'stop\nstart\n');
      } else if (mode === 'stop-fails' || mode.startsWith('cgroup-')) {
        assert.equal(fs.existsSync(path.join(destination, 'trace')), false);
        assert(!result.stdout.includes('easykey_reload_ok'));
        assert(fs.existsSync(path.join(destination, 'running')));
        if (mode.startsWith('cgroup-')) assert(result.stderr.includes('cgroup'));
      } else {
        assert(result.stderr.includes('EasyKey 启动失败'));
      }
    }
    await testReloadLock();
    console.log('Module upgrade and reload tests passed');
  } finally {
    assert.equal(path.dirname(fs.realpathSync(root)), base);
    fs.rmSync(root, { recursive: true, force: true });
  }
}

test().catch(error => { console.error(error); process.exitCode = 1; });
