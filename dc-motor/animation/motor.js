(function(){
const W=1920,H=1080, cv=document.getElementById('c'), ctx=cv.getContext('2d');
const TL=window.TIMELINE, S=TL.scenes, TOTAL=TL.total;
const C={x:620,y:500}, R=205, CR=30, INS={x:1500,y:340,ro:105,ri:64}, GAP=5;
const COL={text:'#e5edf8',dim:'#8ea0bd',A:'#f59e0b',B:'#14b8a6',N:'#ef4444',S:'#3b82f6',force:'#f43f5e',field:'#7dd3fc',acc:'#a78bfa',green:'#4ade80',yellow:'#fde047'};
const FONT='Inter, "DejaVu Sans", Arial, sans-serif';
const clamp=(x,a=0,b=1)=>Math.min(b,Math.max(a,x)), rad=d=>d*Math.PI/180;
const K=4, ease={lin:u=>u,out:u=>1-Math.pow(1-u,3),inout:u=>0.5-0.5*Math.cos(Math.PI*u),
  sat:u=>(u-(1-Math.exp(-K*u))/K)/(1-(1-Math.exp(-K))/K)};
const satRate=d=>d*(1-Math.exp(-K))/(1-(1-Math.exp(-K))/K);   // end speed factor of 'sat' (per second = satRate(d)/T)

// ---------- motion (rotor angle in degrees, CCW positive) ----------
const M=[ // s=scene, a=start at cue a (default scene start), b=end at cue b (default scene end), f0/f1 fractions
 {s:0,delta:720,ease:'out'},
 {s:4,a:1,delta:90,ease:'inout'},
 {s:5,a:2,f0:0,f1:.4,delta:28,ease:'out'},
 {s:5,a:2,f0:.4,f1:1,delta:-28,ease:'inout'},
 {s:6,a:1,delta:500,ease:'inout'},
 {s:7,delta:360*12,ease:'sat'},
];
function seg(m){const s=S[m.s],t0=m.a?s.cues[m.a].start:s.start,t1=m.b!=null?s.cues[m.b].start:s.end,d=t1-t0;
  return {t0:t0+d*(m.f0||0),t1:t0+d*(m.f1==null?1:m.f1),delta:m.delta,ease:m.ease};}
const SEGS=M.map(seg);
{const g=SEGS[SEGS.length-1],T=g.t1-g.t0,rate=satRate(g.delta)/T,s8=S[8];   // keep spinning at S8's final speed in the recap
 SEGS.push({t0:s8.start,t1:s8.end,delta:rate*(s8.end-s8.start),ease:'lin'});}
function theta(t){let th=0;for(const g of SEGS){if(t>=g.t1)th+=g.delta;else if(t>g.t0){th+=g.delta*ease[g.ease]((t-g.t0)/(g.t1-g.t0));break;}else break;}return th;}

// ---------- helpers ----------
function sceneIdx(t){for(let i=S.length-1;i>=0;i--)if(t>=S[i].start)return i;return 0;}
let T=0;                                             // current time
const cueStart=(s,c)=>S[s].cues[c].start;
const reveal=(s,c,d=.7)=>clamp((T-cueStart(s,c))/d);
const cueFrac=(s,c)=>clamp((T-cueStart(s,c))/(S[s].cues[c].end-cueStart(s,c)));
const sceneFade=s=>clamp(Math.min((T-S[s].start)/.5,(S[s].end-T)/.5));
function txt(s,x,y,size,col,align='left',weight=600,alpha=1){ctx.save();ctx.globalAlpha*=alpha;ctx.font=`${weight} ${size}px ${FONT}`;ctx.fillStyle=col;ctx.textAlign=align;ctx.textBaseline='middle';ctx.fillText(s,x,y);ctx.restore();}
function arrow(x1,y1,x2,y2,col,lw=7,head=20){const a=Math.atan2(y2-y1,x2-x1);ctx.strokeStyle=col;ctx.fillStyle=col;ctx.lineWidth=lw;ctx.lineCap='round';
  ctx.beginPath();ctx.moveTo(x1,y1);ctx.lineTo(x2-Math.cos(a)*head*.6,y2-Math.sin(a)*head*.6);ctx.stroke();
  ctx.beginPath();ctx.moveTo(x2,y2);ctx.lineTo(x2-head*Math.cos(a-.42),y2-head*Math.sin(a-.42));ctx.lineTo(x2-head*Math.cos(a+.42),y2-head*Math.sin(a+.42));ctx.closePath();ctx.fill();}
function rrect(x,y,w,h,r){ctx.beginPath();ctx.moveTo(x+r,y);ctx.arcTo(x+w,y,x+w,y+h,r);ctx.arcTo(x+w,y+h,x,y+h,r);ctx.arcTo(x,y+h,x,y,r);ctx.arcTo(x,y,x+w,y,r);ctx.closePath();}
function panel(x,y,w,h,title,alpha){ctx.save();ctx.globalAlpha=alpha;rrect(x,y,w,h,22);ctx.fillStyle='rgba(15,23,42,.72)';ctx.fill();ctx.strokeStyle='rgba(148,163,184,.35)';ctx.lineWidth=2;ctx.stroke();
  txt(title,x+w/2,y+36,28,COL.text,'center',700);ctx.restore();}
function label(text,x,y,tx,ty,align,alpha,col=COL.text){ctx.save();ctx.globalAlpha=alpha;ctx.strokeStyle=col;ctx.lineWidth=2.5;ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(tx,ty);ctx.stroke();
  ctx.fillStyle=col;ctx.beginPath();ctx.arc(x,y,6,0,7);ctx.fill();ctx.restore();txt(text,tx+(align==='left'?12:-12),ty,30,col,align,700,alpha);}

// ---------- electrical state ----------
function topSeg(th){const m=(((90-th)%360)+360)%360;return m<180?'A':'B';}
function gapDist(th){const m=(((90-th)%180)+180)%180;return Math.min(m,180-m);}
function current(th,noComm,si){
  let out,I;
  if(noComm){out=1;I=1;}else{out=topSeg(th)==='A'?1:2;I=gapDist(th)<GAP?0:1;}
  if(si===7){const p=(T-S[7].start)/(S[7].end-S[7].start),s=(1-Math.exp(-K*p))/(1-Math.exp(-K));I*=1-.85*s;}
  return {out,I};
}

// ---------- scene pieces ----------
function bg(){const g=ctx.createLinearGradient(0,0,W,H);g.addColorStop(0,'#0b1220');g.addColorStop(1,'#14223f');ctx.fillStyle=g;ctx.fillRect(0,0,W,H);}

function drawField(si){
  let a=0; if(si===2)a=reveal(2,0); else if(si>2)a=1;
  if(si===2)a=a*(1-.55*clamp((T-(S[2].end-1))/1)); else if(si>2)a=.45;
  if(a<=0)return;
  const dense=si>2?1:reveal(2,2,1.2), ys=[];
  for(let k=-3;k<=3;k++)ys.push(C.y+k*66);
  const extra=ys.map(y=>y+33).slice(0,6);
  const pulse=si===2&&T>cueStart(2,2)?.75+.25*Math.sin((T-cueStart(2,2))*5):1;
  const drawLine=(y,al)=>{ctx.save();ctx.globalAlpha=a*al*pulse;ctx.strokeStyle=COL.field;ctx.lineWidth=si===2&&T>cueStart(2,2)?5:3.5;
    ctx.setLineDash([22,16]);ctx.lineDashOffset=-(T*70)*(si===2&&T<cueStart(2,1)?0:1);
    ctx.beginPath();ctx.moveTo(334,y);ctx.lineTo(884,y);ctx.stroke();ctx.setLineDash([]);
    ctx.fillStyle=COL.field;ctx.beginPath();ctx.moveTo(906,y);ctx.lineTo(880,y-11);ctx.lineTo(880,y+11);ctx.fill();ctx.restore();};
  ys.forEach(y=>drawLine(y,1)); extra.forEach(y=>drawLine(y,dense));
  if(si===2)txt('Magnetic field  B',C.x,246,34,COL.field,'center',700,a*reveal(2,0));
}
function drawMagnets(){
  const mags=[{x:90,c:COL.N,l:'N',c2:'#991b1b'},{x:910,c:COL.S,l:'S',c2:'#1e3a8a'}];
  for(const m of mags){const g=ctx.createLinearGradient(m.x,0,m.x+240,0);g.addColorStop(0,m.c2);g.addColorStop(1,m.c);
    ctx.fillStyle=g;rrect(m.x,270,240,460,18);ctx.fill();ctx.strokeStyle='rgba(255,255,255,.25)';ctx.lineWidth=2;ctx.stroke();
    txt(m.l,m.x+120,500,120,'rgba(255,255,255,.92)','center',800);}
}
function sidePos(th,k){const a=rad(th+(k===2?180:0));return {x:C.x+R*Math.cos(a),y:C.y-R*Math.sin(a)};}
function drawCoil(si,th,cur){
  const p1=sidePos(th,1),p2=sidePos(th,2);
  // shaft / coil
  ctx.strokeStyle='#cbd5e1';ctx.lineWidth=12;ctx.lineCap='round';ctx.beginPath();ctx.moveTo(p1.x,p1.y);ctx.lineTo(p2.x,p2.y);ctx.stroke();
  ctx.strokeStyle='#64748b';ctx.lineWidth=4;ctx.beginPath();ctx.moveTo(p1.x,p1.y);ctx.lineTo(p2.x,p2.y);ctx.stroke();
  const showCur=si>=3&&si<=7&&T>=cueStart(3,0), sym=Math.max(.4,.4+.6*cur.I);
  const sides=[[1,p1],[2,p2]];
  for(const [k,p] of sides){
    ctx.fillStyle=k===1?COL.A:COL.B;ctx.beginPath();ctx.arc(p.x,p.y,CR,0,7);ctx.fill();
    ctx.strokeStyle='rgba(255,255,255,.55)';ctx.lineWidth=3;ctx.stroke();
    if(showCur&&cur.I>.02){
      const a=si===3?reveal(3,0):1, out=(cur.out===k); ctx.save();ctx.globalAlpha=a;ctx.strokeStyle='#fff';ctx.fillStyle='#fff';ctx.lineWidth=5;
      if(out){ctx.beginPath();ctx.arc(p.x,p.y,8*sym+2,0,7);ctx.fill();}
      else{const d=14*sym+2;ctx.beginPath();ctx.moveTo(p.x-d,p.y-d);ctx.lineTo(p.x+d,p.y+d);ctx.moveTo(p.x+d,p.y-d);ctx.lineTo(p.x-d,p.y+d);ctx.stroke();}
      ctx.restore();}
  }
  ctx.fillStyle='#94a3b8';ctx.beginPath();ctx.arc(C.x,C.y,22,0,7);ctx.fill();ctx.strokeStyle='#e2e8f0';ctx.lineWidth=4;ctx.stroke();
  // forces
  if(si>=3&&si<=7&&cur.I>.02){
    let aOut=1,aIn=1;
    if(si===3){aOut=reveal(3,3);aIn=reveal(3,4);}
    const L=125*(si===7?cur.I:1);
    for(const [k,p] of sides){
      const out=cur.out===k, al=out?aOut:aIn; if(al<=0)continue;
      const dir=out?-1:1;           // out-of-page current -> force up (screen -y)
      ctx.save();ctx.globalAlpha=al;
      arrow(p.x,p.y+dir*(CR+6),p.x,p.y+dir*(CR+6+L),COL.force,9,26);
      txt('F',p.x+22,p.y+dir*(CR+6+L)-dir*14,30,COL.force,'left',800);ctx.restore();}
  }
}
function drawCallouts(si,th){
  if(si===3){
    const f=sceneFade(3), p1=sidePos(th,1),p2=sidePos(th,2);
    // highlight rings
    const ring=(p,al)=>{if(al<=0)return;ctx.save();ctx.globalAlpha=al*(.6+.4*Math.sin(T*6));ctx.strokeStyle='#fff';ctx.lineWidth=4;ctx.beginPath();ctx.arc(p.x,p.y,CR+12,0,7);ctx.stroke();ctx.restore();};
    if(T>=cueStart(3,1)&&T<cueStart(3,2))ring(p1,1);
    if(T>=cueStart(3,4)&&T<cueStart(3,5))ring(p2,1);
    txt('Current',p1.x,p1.y+78,26,COL.A,'center',700,reveal(3,1)*f);txt('OUT (dot)',p1.x,p1.y+108,26,COL.A,'center',700,reveal(3,1)*f);
    txt('Current',p2.x,p2.y-108,26,COL.B,'center',700,reveal(3,4)*f);txt('IN (cross)',p2.x,p2.y-78,26,COL.B,'center',700,reveal(3,4)*f);
  }
  if(si===4){const f=sceneFade(4)*reveal(4,1);ctx.save();ctx.globalAlpha=f;ctx.strokeStyle='#facc15';ctx.lineWidth=9;ctx.lineCap='round';
    ctx.beginPath();ctx.arc(C.x,C.y,262,-rad(10),-rad(75),true);ctx.stroke();
    const a=-rad(75),ex=C.x+262*Math.cos(a),ey=C.y+262*Math.sin(a);arrow(ex+14,ey+22,ex-12,ey-8,'#facc15',9,26);ctx.restore();
    txt('Torque',C.x+205,C.y-262,34,'#facc15','left',800,f);}
  if(si===5){const f=sceneFade(5);
    if(T>=cueStart(5,1)&&T<cueStart(5,2)+.1)txt('Torque = 0',C.x,830,48,COL.force,'center',800,reveal(5,1)*f);
    if(T>=cueStart(5,2)){txt('Without reversing the current…',C.x,830,38,COL.force,'center',700,reveal(5,2)*f);}}
  if(si===6){const f=sceneFade(6);
    if(T>=cueStart(6,3))txt('Right side: always current OUT  →  always pushed UP',C.x,830,36,COL.green,'center',700,reveal(6,3)*f);
    const gd=gapDist(theta(T));if(gd<16&&T>cueStart(6,1))txt('Current reverses!',INS.x-INS.ro-50,INS.y,32,COL.yellow,'right',800,(1-gd/16)*f);
    if(T<cueStart(6,3)){const al=reveal(6,0)*(1-reveal(6,3));ctx.save();ctx.globalAlpha=al*(.5+.5*Math.sin(T*6));ctx.strokeStyle=COL.acc;ctx.lineWidth=6;ctx.beginPath();ctx.arc(INS.x,INS.y,INS.ro+30,0,7);ctx.stroke();ctx.restore();}}
}

// commutator inset + battery
function pathLen(p){let L=0;for(let i=1;i<p.length;i++)L+=Math.hypot(p[i][0]-p[i-1][0],p[i][1]-p[i-1][1]);return L;}
function pathAt(p,d){for(let i=1;i<p.length;i++){const l=Math.hypot(p[i][0]-p[i-1][0],p[i][1]-p[i-1][1]);if(d<=l){const u=d/l;return [p[i-1][0]+(p[i][0]-p[i-1][0])*u,p[i-1][1]+(p[i][1]-p[i-1][1])*u];}d-=l;}return p[p.length-1];}
function drawInset(si,th,cur,noComm){
  const {x,y,ro,ri}=INS, top=[[1760,315],[1760,150],[x,150],[x,190]], bot=[[x,490],[x,530],[1760,530],[1760,345]];
  ctx.strokeStyle='#64748b';ctx.lineWidth=6;ctx.lineJoin='round';
  for(const p of [top,bot]){ctx.beginPath();p.forEach((q,i)=>i?ctx.lineTo(q[0],q[1]):ctx.moveTo(q[0],q[1]));ctx.stroke();}
  // battery
  ctx.strokeStyle='#e2e8f0';ctx.lineWidth=8;ctx.beginPath();ctx.moveTo(1725,315);ctx.lineTo(1795,315);ctx.stroke();
  ctx.lineWidth=14;ctx.beginPath();ctx.moveTo(1740,345);ctx.lineTo(1780,345);ctx.stroke();
  txt('+',1818,312,34,'#fca5a5','left',800);txt('−',1818,347,38,'#93c5fd','left',800);
  // segments
  const g=GAP;
  const segPath=(a0,a1)=>{ctx.beginPath();ctx.arc(x,y,ro,-rad(a1),-rad(a0),false);ctx.arc(x,y,ri,-rad(a0),-rad(a1),true);ctx.closePath();};
  const ts=topSeg(th), live=si>=3&&si<=7&&T>=cueStart(3,0)&&cur.I>.02;
  for(const k of ['A','B']){const base=k==='A'?th:th+180, col=k==='A'?COL.A:COL.B;
    segPath(base+g,base+180-g);ctx.fillStyle=col;ctx.globalAlpha=(live&&ts===k)?1:.62;ctx.fill();ctx.globalAlpha=1;
    ctx.strokeStyle='rgba(255,255,255,.5)';ctx.lineWidth=3;ctx.stroke();}
  ctx.fillStyle='#1e293b';ctx.beginPath();ctx.arc(x,y,ri-4,0,7);ctx.fill();ctx.strokeStyle='#94a3b8';ctx.lineWidth=3;ctx.stroke();
  ctx.fillStyle='#94a3b8';ctx.beginPath();ctx.arc(x,y,14,0,7);ctx.fill();
  // brushes
  for(const [yy,h,c] of [[y-ro-52,56,'#fca5a5'],[y+ro-4,56,'#93c5fd']]){ctx.fillStyle='#475569';rrect(x-24,yy,48,h,6);ctx.fill();ctx.strokeStyle='#cbd5e1';ctx.lineWidth=2.5;ctx.stroke();}
  txt('+',x+46,y-ro-30,34,'#fca5a5','left',800);txt('−',x+46,y+ro+30,38,'#93c5fd','left',800);
  txt('Commutator & brushes (end view)',x+20,568,22,COL.dim,'center',500);
  // current flow
  if(live){ctx.fillStyle=COL.yellow;const off=(T*110)%46;
    for(const p of [top,bot]){const L=pathLen(p);for(let d=off;d<L;d+=46){const q=pathAt(p,d);ctx.save();ctx.globalAlpha=.35+.65*cur.I;ctx.beginPath();ctx.arc(q[0],q[1],5.5,0,7);ctx.fill();ctx.restore();}}}
  // S2 labels
  if(si===1){const f=sceneFade(1);
    if(T>=cueStart(1,3))label('Commutator',x-ro+8,y,x-ro-60,y,'right',reveal(1,3)*f,COL.acc);
    if(T>=cueStart(1,4)){label('Brushes',x+24,y-ro-24,x+80,y-ro-24,'left',reveal(1,4)*f,'#cbd5e1');
      txt('Battery',1760,418,30,'#e2e8f0','center',700,reveal(1,4)*f);}
  }
}

function drawPanels(si,th,noComm){
  const X=1230,Y=582,Pw=630,Ph=318;
  // Fleming + formula (scene 3)
  if(si===3&&T>=cueStart(3,2)){
    const f=sceneFade(3)*reveal(3,2), form=reveal(3,5);
    panel(X,Y,Pw,Ph,form>0.5?'Size of the force':"Fleming's left-hand rule",f);
    txt('Hold out your left hand:',X+Pw/2,Y+130,30,COL.text,'center',600,f*(1-form)*(1-clamp(cueFrac(3,3)*3)));txt('first finger, second finger, thumb at right angles',X+Pw/2,Y+170,22,COL.dim,'center',500,f*(1-form)*(1-clamp(cueFrac(3,3)*3)));
    ctx.save();ctx.globalAlpha=f*(1-form);
    const O={x:1440,y:815}, f3=cueFrac(3,3), aB=clamp(f3*3), aF=clamp(f3*3-1), aI=clamp(f3*3-2);
    ctx.save();ctx.globalAlpha*=T<cueStart(3,3)?0:Math.max(aB,.001);arrow(O.x,O.y,O.x+215,O.y,COL.field,8,24);ctx.restore();
    txt('Field B',O.x+232,O.y-14,28,COL.field,'left',700,aB);txt('1st finger',O.x+232,O.y+16,22,COL.dim,'left',500,aB);
    ctx.save();ctx.globalAlpha*=aI;arrow(O.x,O.y,O.x-95,O.y+64,COL.A,8,24);ctx.restore();
    txt('Current I',O.x-108,O.y+30,28,COL.A,'right',700,aI);txt('2nd finger',O.x-108,O.y+58,22,COL.dim,'right',500,aI);
    ctx.save();ctx.globalAlpha*=aF;arrow(O.x,O.y,O.x,O.y-130,COL.force,8,24);ctx.restore();
    txt('Force F  (thumb)',O.x+14,O.y-150,28,COL.force,'left',700,aF);
    ctx.restore();
    ctx.save();ctx.globalAlpha=f*form;txt('F = B × I × L',X+Pw/2,Y+160,70,COL.text,'center',800);
    txt('B  field strength',X+Pw/2,Y+232,28,COL.field,'center',600);txt('I  current',X+Pw/2,Y+266,28,COL.A,'center',600);txt('L  length of wire in the field',X+Pw/2,Y+300,28,COL.dim,'center',600);ctx.restore();
  }
  // torque graph (scenes 4-6)
  if(si>=4&&si<=6){
    const f=si===4?clamp((T-S[4].start)/.5):si===6?clamp((S[6].end-T)/.5):1;
    panel(X,Y,Pw,Ph,'Torque vs coil angle',f);ctx.save();ctx.globalAlpha=f;
    const gx0=X+60,gx1=X+Pw-30,gy=Y+190,sy=80,px=a=>gx0+a/360*(gx1-gx0),py=v=>gy-v*sy;
    ctx.strokeStyle='rgba(148,163,184,.5)';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(gx0,gy);ctx.lineTo(gx1,gy);ctx.moveTo(gx0,py(1.15));ctx.lineTo(gx0,py(-1.15));ctx.stroke();
    for(const a of [0,90,180,270,360]){txt(a+'°',px(a),Y+Ph-32,22,COL.dim,'center',500);ctx.beginPath();ctx.moveTo(px(a),gy-5);ctx.lineTo(px(a),gy+5);ctx.stroke();}
    txt('+',gx0-22,py(.9),28,COL.dim,'center');txt('−',gx0-22,py(-.9),28,COL.dim,'center');
    const curve=(fn,col,lw,al)=>{ctx.save();ctx.globalAlpha*=al;ctx.strokeStyle=col;ctx.lineWidth=lw;ctx.lineJoin='round';ctx.beginPath();for(let a=0;a<=360;a+=3){const q=fn(a);a?ctx.lineTo(px(a),py(q)):ctx.moveTo(px(a),py(q));}ctx.stroke();ctx.restore();};
    const cosf=a=>Math.cos(rad(a)), absf=a=>Math.abs(Math.cos(rad(a)));
    const comm=si===6?reveal(6,2):0;
    curve(cosf,'#38bdf8',5,1-.65*comm);
    if(comm>0)curve(absf,COL.green,6,comm);
    const a=((theta(T)%360)+360)%360, v=noComm?cosf(a):(comm>0?absf(a):cosf(a));
    ctx.fillStyle='#fff';ctx.shadowColor='#fff';ctx.shadowBlur=16;ctx.beginPath();ctx.arc(px(a),py(v),10,0,7);ctx.fill();ctx.shadowBlur=0;
    if(si===5&&T>=cueStart(5,2))txt('negative torque pushes it back',X+Pw/2,Y+68,24,COL.force,'center',700,reveal(5,2));
    if(si===6)txt('torque always positive',X+Pw/2,Y+68,24,COL.green,'center',700,comm);
    ctx.restore();
  }
  // speed / back emf / current (scene 7)
  if(si===7){
    const f=sceneFade(7);panel(X,Y,Pw,Ph,'Speed, back EMF and current',f);ctx.save();ctx.globalAlpha=f;
    const gx0=X+50,gx1=X+Pw-30,gy=Y+Ph-52,sy=128,px=u=>gx0+u*(gx1-gx0),py=v=>gy-v*sy,p=(T-S[7].start)/(S[7].end-S[7].start);
    ctx.strokeStyle='rgba(148,163,184,.5)';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(gx0,gy);ctx.lineTo(gx1,gy);ctx.moveTo(gx0,gy);ctx.lineTo(gx0,py(1.05));ctx.stroke();
    txt('time →',gx1,gy+26,22,COL.dim,'right',500);
    const s=u=>(1-Math.exp(-K*u))/(1-Math.exp(-K)), cv_=(fn,col,al)=>{ctx.save();ctx.globalAlpha*=al;ctx.strokeStyle=col;ctx.lineWidth=6;ctx.lineJoin='round';ctx.beginPath();for(let u=0;u<=p;u+=.01){u?ctx.lineTo(px(u),py(fn(u))):ctx.moveTo(px(u),py(fn(u)));}ctx.stroke();ctx.restore();};
    const aE=reveal(7,1), aI=reveal(7,2);
    cv_(s,'#60a5fa',aE);cv_(s,COL.acc,0);cv_(u=>1-.85*s(u),'#fb923c',aI);
    txt('Speed  ( = back EMF, grows together)',X+30,Y+68,24,'#60a5fa','left',700,aE);txt('Current  (falls as back EMF rises)',X+30,Y+98,24,'#fb923c','left',700,aI);
    if(T>=cueStart(7,3)){const m=reveal(7,3);txt('start: large current',px(.02)+8,py(1)-10,24,'#fb923c','left',700,m);txt('running: small current',px(.97),py(.15)-34,24,'#fb923c','right',700,m);}
    ctx.restore();
  }
  // recap (scene 8)
  if(si===8){
    const gAl=reveal(8,0)*(1-reveal(8,1)), lAl=reveal(8,1);
    if(gAl>0){panel(X,Y,Pw,Ph,'Smoother torque with more coils',gAl);ctx.save();ctx.globalAlpha=gAl;
      const gx0=X+50,gx1=X+Pw-30,gy=Y+Ph-52,sy=105,px=a=>gx0+a/360*(gx1-gx0),py=v=>gy-v*sy;
      ctx.strokeStyle='rgba(148,163,184,.5)';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(gx0,gy);ctx.lineTo(gx1,gy);ctx.stroke();
      const one=a=>Math.abs(Math.cos(rad(a))),three=a=>(one(a)+one(a+60)+one(a+120))/1.7;
      const crv=(fn,col,lw)=>{ctx.strokeStyle=col;ctx.lineWidth=lw;ctx.beginPath();for(let a=0;a<=360;a+=3)a?ctx.lineTo(px(a),py(fn(a))):ctx.moveTo(px(a),py(fn(a)));ctx.stroke();};
      crv(one,'rgba(74,222,128,.55)',4);crv(three,'#fff',6);
      txt('1 coil',gx0+10,py(1)-12,24,COL.green,'left',700);txt('3 coils (smooth)',gx1,py(1.0)-32,24,'#fff','right',700);
      ctx.restore();}
    if(lAl>0){panel(X,Y,Pw,Ph,'Key ideas',lAl);
      const items=[['Current in a magnetic field → force',COL.force,1],['Two opposite forces → torque',COL.A,2],['Commutator → torque never reverses',COL.green,3]];
      items.forEach(([s,c,ci],i)=>{const a=reveal(8,ci)*lAl;ctx.save();ctx.globalAlpha=a;ctx.fillStyle=c;ctx.beginPath();ctx.arc(X+52,Y+105+i*70,20,0,7);ctx.fill();ctx.restore();
        txt(String(i+1),X+52,Y+106+i*70,24,'#0b1220','center',800,a);txt(s,X+86,Y+105+i*70,27,COL.text,'left',600,a);});}
  }
}

// ---------- title, captions, progress ----------
function title(si){
  let t=S[si].title, sub='';
  if(si===0)sub='A step-by-step visual explanation';
  if(si===8&&T>=cueStart(8,4))t='Thanks for watching!';
  const fade=sceneFade(si)>0?1:1, prev=null;
  const a=clamp(Math.min((T-S[si].start)/.5,1));
  txt(t,90,86,si===0?78:62,'#fff','left',800,a);
  if(sub)txt(sub,94,150,32,COL.dim,'left',500,a);
}
function caption(){
  let cue=null;for(const sc of S)for(const c of sc.cues)if(T>=c.start)cue=c;
  if(!cue||T>cue.end+.35)return;
  const a=clamp((T-cue.start)/.25)*clamp((cue.end+.35-T)/.25);
  ctx.save();ctx.globalAlpha=a;ctx.font=`600 38px ${FONT}`;
  const words=cue.text.split(' '),lines=[];let cur='';
  for(const w of words){const tst=cur?cur+' '+w:w;if(ctx.measureText(tst).width>1560&&cur){lines.push(cur);cur=w;}else cur=tst;}
  lines.push(cur);const lh=52,bh=lines.length*lh+26,by=1050-bh,bw=Math.max(...lines.map(l=>ctx.measureText(l).width))+70;
  rrect(960-bw/2,by,bw,bh,20);ctx.fillStyle='rgba(2,6,23,.78)';ctx.fill();
  ctx.fillStyle='#fff';ctx.textAlign='center';ctx.textBaseline='middle';lines.forEach((l,i)=>ctx.fillText(l,960,by+13+lh/2+i*lh));ctx.restore();
}
function progress(){ctx.fillStyle='rgba(148,163,184,.25)';ctx.fillRect(0,1074,W,6);ctx.fillStyle=COL.acc;ctx.fillRect(0,1074,W*clamp(T/TOTAL),6);}

function s2labels(si,th){
  if(si!==1)return;const f=sceneFade(1);
  if(T>=cueStart(1,1)){const a=reveal(1,1)*f;
    ctx.save();ctx.globalAlpha=a;ctx.strokeStyle='#e2e8f0';ctx.lineWidth=2.5;ctx.beginPath();ctx.moveTo(210,262);ctx.lineTo(380,212);ctx.moveTo(1030,262);ctx.lineTo(860,212);ctx.stroke();ctx.restore();
    txt('Stator: permanent magnets',C.x,206,32,'#e2e8f0','center',700,a);}
  if(T>=cueStart(1,2)){const p=sidePos(th,2);label('Armature coil on a shaft',p.x,p.y+CR,p.x+30,820,'left',reveal(1,2)*f,COL.B);}
}

function render(t){
  T=Math.min(Math.max(t,0),TOTAL-1e-3);
  const si=sceneIdx(T), th=theta(T), noComm=si<=5, cur=current(th,noComm,si);
  ctx.setTransform(1,0,0,1,0,0);ctx.globalAlpha=1;
  bg();drawField(si);drawMagnets();drawCoil(si,th,cur);drawCallouts(si,th);s2labels(si,th);drawInset(si,th,cur,noComm);drawPanels(si,th,noComm);
  title(si);caption();progress();
  const fo=clamp((TOTAL-T)/1.5),fi=clamp(T/.6);ctx.fillStyle=`rgba(0,0,0,${1-Math.min(fo,fi)})`;ctx.fillRect(0,0,W,H);
}
window.renderFrame=render;window.TOTAL=TOTAL;
const q=new URLSearchParams(location.search);
if(q.has('t'))render(parseFloat(q.get('t')));
else{let st=null;const loop=ts=>{if(st===null)st=ts;render((ts-st)/1000);if((ts-st)/1000<TOTAL)requestAnimationFrame(loop);};requestAnimationFrame(loop);}
window.__ready=true;
})();
