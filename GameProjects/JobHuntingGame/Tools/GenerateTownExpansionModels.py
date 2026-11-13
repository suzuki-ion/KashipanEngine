"""Generate original low-poly town expansion building models, in metres."""
from GenerateTownModels import Mesh

def build():
    for name,floors,width in [('Apartment',3,1.2),('Highrise',7,2.35),('Supermarket',1,1.35),('Mall',3,2.5),('GuildMedium',3,1.3),('GuildLarge',4,2.4),('Police',2,1.2)]:
        m=Mesh();h=floors*.48;m.box(0,h/2,0,width,h,width*.85);m.box(0,h+.06,0,width+.12,.12,width*.9)
        for f in range(floors):
            for x in [-.3,.3]:m.box(x*width,(f+.5)*.48,-width*.43,.19,.24,.05)
        m.box(0,.3,-width*.45,.25,.6,.08)
        if name=='Police':m.box(0,h+.3,0,.45,.35,.16)
        m.save(name)
    for recipe in range(3):
        for tier in range(3):
            m=Mesh();w=1.1 if tier<2 else 2.35;h=.8+tier*.35;m.box(0,h/2,0,w,h,w*.85);m.roof(0,h,0,w+.1,.25,w*.9)
            for i in range(recipe+1):m.cylinder(-w*.3+i*w*.26,h*.75,w*.2,.10,h+.4,segments=8)
            m.box(0,.28,-w*.45,.4,.56,.08);m.save('Factory'+str(recipe)+'Tier'+str(tier))
if __name__=='__main__':build()
