// Usage: node shot.js outDir   (renders each page with the mock bridge; prints console errors)
const { chromium } = require('playwright'); const path = require('path'); const fs = require('fs');
(async () => {
  const out = process.argv[2] || '.'; fs.mkdirSync(out, { recursive: true });
  const b = await chromium.launch(); const errs = [];
  for (const scheme of ['light', 'dark']) {
    const ctx = await b.newContext({ viewport: { width: 1360, height: 780 }, colorScheme: scheme, deviceScaleFactor: 1 }); const p = await ctx.newPage();
    p.on('pageerror', e => errs.push(scheme + ' pageerror: ' + e.message)); p.on('console', m => { if (m.type() === 'error') errs.push(scheme + ' console: ' + m.text()); });
    await p.addInitScript({ path: path.join(__dirname, 'mock.js') }); await p.goto('file://' + path.resolve(__dirname, '../../ui/index.html')); await p.waitForTimeout(300);
    const shot = async n => { await p.waitForTimeout(250); await p.screenshot({ path: path.join(out, scheme + '-' + n + '.png') }); };
    if (scheme === 'light') {
      await p.click('[data-act=fullCheck]'); await p.waitForTimeout(600); await shot('today');
      await p.click('[data-go=clean]'); await p.waitForTimeout(300); await p.click('[data-clean="0"]'); await p.click('[data-act=cleanRec]'); await shot('clean');
      await p.click('[data-act=cleanRun]'); await shot('clean-confirm'); await p.click('[data-r="0"]');
      await p.click('[data-go=programs]'); await p.waitForTimeout(300); await p.click('[data-prog=p0]'); await shot('programs');
      await p.click('[data-act=uninstall][data-key=p0]'); await shot('uninstall-choice'); await p.click('[data-r="2"]'); await p.waitForTimeout(300); await shot('leftovers'); await p.click('[data-r="0"]');
      await p.click('[data-go=startup]'); await p.waitForTimeout(300); await p.click('[data-su=k2]'); await shot('startup');
      await p.click('[data-go=services]'); await p.waitForTimeout(300); await p.click('[data-svc=wuauserv]'); await shot('services');
      await p.click('[data-go=files]'); await p.click('[data-ft=dupes]'); await p.click('[data-act=fileScan]'); await p.waitForTimeout(300); await p.click('[data-act=dupKeep][data-v=newest]'); await shot('dupes');
      await p.click('[data-go=storage]'); await p.waitForTimeout(300); await p.click('[data-drive="C:\\\\"]'); await p.waitForTimeout(300); await shot('storage');
      await p.click('[data-go=journal]'); await p.waitForTimeout(300); await shot('journal');
      await p.click('[data-go=settings]'); await shot('settings');
    } else { await p.click('[data-act=fullCheck]'); await p.waitForTimeout(600); await shot('today'); await p.click('[data-go=startup]'); await p.waitForTimeout(300); await shot('startup'); }
    await ctx.close();
  }
  await b.close(); console.log(errs.length ? errs.join('\n') : 'no console errors');
})();
