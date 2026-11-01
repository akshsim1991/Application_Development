// Usage: node render_frames.js <projectDir> <fps> <workers>
const {chromium}=require('playwright');const fs=require('fs');
const [dir,fps0,workers0]=process.argv.slice(2),fps=+fps0||30,W=+workers0||4;
(async()=>{
 const out=dir+'/build/frames';fs.mkdirSync(out,{recursive:true});
 const b=await chromium.launch();
 const probe=await b.newPage();await probe.goto('file://'+dir+'/animation/index.html?t=0');
 const total=await probe.evaluate(()=>window.TOTAL);const N=Math.floor(total*fps);await probe.close();
 console.log('frames',N);let next=0,done=0;
 async function worker(){const p=await b.newPage({viewport:{width:1920,height:1080}});
  await p.goto('file://'+dir+'/animation/index.html?t=0');await p.waitForFunction('window.__ready');
  for(;;){const i=next++;if(i>=N)break;await p.evaluate(t=>window.renderFrame(t),i/fps);
   await p.screenshot({path:`${out}/f${String(i).padStart(5,'0')}.jpg`,type:'jpeg',quality:92});
   if(++done%300===0)console.log(done,'/',N);}}
 await Promise.all(Array.from({length:W},worker));await b.close();console.log('done');
})();
