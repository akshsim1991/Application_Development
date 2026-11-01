const {chromium}=require('playwright');
(async()=>{const b=await chromium.launch();const p=await b.newPage({viewport:{width:1920,height:1080}});
p.on('pageerror',e=>console.log('ERR',e.message));p.on('console',m=>console.log('LOG',m.text()));
await p.goto('file://'+process.argv[2]+'/animation/index.html?t=0');await p.waitForFunction('window.__ready');
for(const t of process.argv.slice(3)){await p.evaluate(t=>window.renderFrame(t),+t);await p.screenshot({path:`/tmp/shot_${t}.png`});}
await b.close();})();
