"""Generate a light, original ambient background track (soft pad + gentle plucks + bass) with numpy."""
import json, os, numpy as np, soundfile as sf
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
total=json.load(open(f"{ROOT}/script/timeline.json"))["total"]
sr=44100; n=int((total+2)*sr); bpm=72; beat=60/bpm; bar=4*beat
rng=np.random.default_rng(7)
hz=lambda m:440*2**((m-69)/12)
prog=[([48,55,59,64],[60,64,67,71]),([45,52,55,60],[57,60,64,67]),([41,48,52,57],[53,57,60,64]),([43,50,53,59],[55,59,62,65])]  # Cmaj7 Am7 Fmaj7 G6
L=np.zeros(n);Rr=np.zeros(n)
def add(sig,start,pan=0.5):
    i=int(start*sr);j=min(n,i+len(sig))
    if i>=n:return
    L[i:j]+=sig[:j-i]*(1-pan);Rr[i:j]+=sig[:j-i]*pan
def env(m,a,d):
    t=np.arange(m)/sr;return np.minimum(t/a,1)*np.exp(-t/d)
nb=int(total/bar)+2
for b in range(nb):
    bass,ch=prog[b%4];t0=b*bar
    # pad: each chord tone, slow swell, slight detune
    m=int((bar+1.5)*sr);t=np.arange(m)/sr
    e=np.minimum(t/1.2,1)*np.clip((bar+1.5-t)/1.5,0,1)
    for k,nt in enumerate(ch):
        sig=sum(np.sin(2*np.pi*hz(nt)*(1+dt)*t) for dt in(-.002,0,.002))/3*e*0.05
        add(sig,t0,0.3+0.13*k)
    # bass root
    mb=int(bar*sr);tb=np.arange(mb)/sr
    add(np.sin(2*np.pi*hz(bass[0])*tb)*env(mb,.02,1.6)*0.10,t0)
    # arpeggio plucks (eighths)
    pat=[0,1,2,3,2,1,2,3]
    for e8,pi in enumerate(pat):
        nt=ch[pi]+12;ml=int(1.6*sr);tt=np.arange(ml)/sr
        sig=(np.sin(2*np.pi*hz(nt)*tt)+.3*np.sin(2*np.pi*hz(nt)*2*tt)*np.exp(-tt*4))*env(ml,.004,.55)*0.07
        add(sig,t0+e8*beat/2,0.25+0.5*(pi/3))
# reverb via exponentially decaying noise IR
ir_len=int(2.2*sr);ir=rng.standard_normal(ir_len)*np.exp(-np.arange(ir_len)/(0.55*sr));ir[0]=0
def verb(x):
    N=1<<int(np.ceil(np.log2(len(x)+ir_len)));y=np.fft.irfft(np.fft.rfft(x,N)*np.fft.rfft(ir,N),N)[:len(x)]
    return x*0.65+y*0.07
L2,R2=verb(L),verb(Rr)
out=np.stack([L2,R2],1)[:int(total*sr)]
fi=int(3*sr);fo=int(5*sr);out[:fi]*=np.linspace(0,1,fi)[:,None];out[-fo:]*=np.linspace(1,0,fo)[:,None]
out*=0.8/np.abs(out).max()
sf.write(f"{ROOT}/build/music.wav",out.astype(np.float32),sr);print("music ok",total)
