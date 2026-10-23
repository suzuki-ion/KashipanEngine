"""Generate six original low-poly facility OBJ assets (metres, Y up)."""
from pathlib import Path
from math import sin, cos, pi, sqrt

OUTPUT = Path(__file__).resolve().parents[1] / 'Assets/Application/Models/Town'

class Mesh:
    def __init__(self):
        self.lines = ['# Light Town original facility model', 's off']
        self.vertices = self.normals = 0

    def face(self, points):
        a, b, c = points[:3]
        u = [b[i]-a[i] for i in range(3)]; v = [c[i]-a[i] for i in range(3)]
        n = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        length = sqrt(sum(x*x for x in n)); assert length > 1e-9
        self.normals += 1
        self.lines.append('vn ' + ' '.join(f'{x/length:.6f}' for x in n))
        first = self.vertices + 1
        for p in points:
            self.lines.append('v ' + ' '.join(f'{x:.6f}' for x in p)); self.vertices += 1
        for i in range(1, len(points)-1):
            self.lines.append('f ' + ' '.join(f'{j}//{self.normals}' for j in [first, first+i, first+i+1]))

    def box(self, x, y, z, w, h, d):
        p = [(x+sx*w/2,y+sy*h/2,z+sz*d/2) for sx,sy,sz in
             [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        for f in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),(0,4,7,3),(1,2,6,5)]: self.face([p[i] for i in f])

    def roof(self, x, y, z, w, h, d):
        p=[(x-w/2,y,z-d/2),(x+w/2,y,z-d/2),(x,y+h,z-d/2),
           (x-w/2,y,z+d/2),(x+w/2,y,z+d/2),(x,y+h,z+d/2)]
        for f in [(0,2,1),(3,4,5),(0,3,5,2),(1,2,5,4),(0,1,4,3)]: self.face([p[i] for i in f])

    def cylinder(self, x, y, z, radius, h, top=None, segments=12):
        top = radius if top is None else top
        low=[(x+cos(i*2*pi/segments)*radius,y,z+sin(i*2*pi/segments)*radius) for i in range(segments)]
        high=[(x+cos(i*2*pi/segments)*top,y+h,z+sin(i*2*pi/segments)*top) for i in range(segments)]
        for i in range(segments):
            j=(i+1)%segments; self.face([low[i],high[i],high[j],low[j]])
        self.face(low); self.face(high[::-1])

    def save(self, name):
        OUTPUT.mkdir(parents=True,exist_ok=True)
        (OUTPUT/(name+'.obj')).write_text('\n'.join(self.lines)+'\n',encoding='utf-8')

def build():
    m=Mesh(); m.cylinder(0,0,0,.23,.14); m.cylinder(0,.14,0,.07,1.96)
    m.box(.15,2.15,0,.44,.08,.1); m.box(.3,2.03,0,.28,.3,.28)
    m.cylinder(.3,2.19,0,.23,.16,.04); m.cylinder(.3,1.8,0,.05,.08,.16); m.save('Streetlight')

    m=Mesh(); m.box(0,.08,0,1.16,.16,1.12); m.box(0,.66,0,1,1.1,1)
    m.roof(0,1.21,0,1.24,.48,1.2); m.box(.3,1.57,.22,.17,.5,.18)
    for x in [-.3,.3]: m.box(x,.65,-.515,.24,.3,.06)
    m.box(0,.37,-.535,.22,.65,.08); m.box(0,.12,-.62,.42,.12,.22); m.save('House')

    m=Mesh(); m.box(0,.08,0,1.2,.16,1.12); m.box(-.08,.6,0,1.0,1.05,1)
    m.roof(-.08,1.12,0,1.12,.2,1.08); m.box(.42,.9,.2,.25,1.8,.26)
    m.box(.42,1.79,.2,.34,.12,.34); m.box(-.2,.46,-.55,.44,.64,.12)
    for x in [-.42,-.1,.22]: m.box(x,.94,-.55,.18,.19,.1)
    m.cylinder(-.48,.16,-.4,.16,.38); m.save('Workshop')

    m=Mesh(); m.box(0,.06,0,1.2,.12,1.12); m.box(0,.68,.07,1.06,1.24,.9)
    m.box(0,1.35,.06,1.2,.16,1.05); m.box(0,.86,-.57,1.22,.12,.45)
    for x in [-.52,-.26,0,.26,.52]: m.box(x,.73,-.75,.2,.18,.08)
    for x in [-.52,.52]: m.box(x,.39,-.72,.05,.76,.05)
    m.box(0,1.65,.04,.85,.34,.09)
    for x in [-.46,.25]: m.box(x,.43,-.42,.33,.58,.08)
    m.save('Shop')

    m=Mesh(); m.box(0,.1,0,1.2,.2,1.15); m.box(0,.79,0,1.04,1.38,.94)
    m.roof(0,1.48,0,1.22,.4,1.1)
    for x in [-.46,.46]:
        m.box(x,.75,-.51,.13,1.3,.2); m.box(x,1.45,-.51,.22,.12,.26)
    m.box(0,.52,-.53,.38,.9,.09); m.box(0,1.12,-.58,.34,.35,.1)
    m.cylinder(.47,1.66,.25,.035,.75)
    m.box(.3,2.22,.25,.35,.26,.06); m.save('Guild')

    m=Mesh(); m.cylinder(0,0,0,.62,.18,segments=8); m.cylinder(0,.18,0,.46,.3,segments=8)
    m.cylinder(0,.48,0,.31,2.5,.22,8)
    for y in [.55,1.4,2.45,2.95]: m.cylinder(0,y,0,.37,.12,segments=8)
    for x,z in [(-.42,0),(.42,0),(0,-.42),(0,.42)]: m.box(x,.8,z,.12,1.5,.12)
    m.cylinder(0,3.07,0,.38,.55,.22,8); m.cylinder(0,3.62,0,.22,.55,.025,8)
    for x,z in [(-.32,0),(.32,0),(0,-.32),(0,.32)]: m.box(x,3.54,z,.08,1.1,.08)
    m.cylinder(0,4.12,0,.4,.55,.025,8); m.save('Tower')

if __name__=='__main__':
    build(); print('Generated six original facility OBJ models in',OUTPUT)
