"""Original, low-poly harvest nodes; Y-up, base at y=0."""
from math import sin, cos, pi
from pathlib import Path
import GenerateTownModels as shapes

shapes.OUTPUT=Path(__file__).resolve().parents[1]/'Assets/Application/Models/Resources'

def log(mesh,x,y,z,radius,length):
    rings=[[(x+cos(i*pi/5)*radius,y+sin(i*pi/5)*radius,z+side*length/2) for i in range(10)] for side in [-1,1]]
    for i in range(10):
        j=(i+1)%10;mesh.face([rings[0][i],rings[0][j],rings[1][j],rings[1][i]])
    mesh.face(rings[0][::-1]);mesh.face(rings[1])
    # Cut-end rings stand slightly proud of the bark.
    for zz in [z-length/2-.012,z+length/2+.012]:
        log_end=[(x+cos(i*pi/5)*radius*.76,y+sin(i*pi/5)*radius*.76,zz) for i in range(10)]
        mesh.face(log_end[::-1] if zz<z else log_end)

def boulder(mesh,x,y,z,w,h,d):
    ring=[(x+cos(i*2*pi/7)*w/2,y+h*.3,z+sin(i*2*pi/7)*d/2) for i in range(7)]
    top=[(x+cos(i*2*pi/7+.2)*w*.34,y+h*.8+(i%2)*h*.09,z+sin(i*2*pi/7+.2)*d*.34) for i in range(7)]
    for i in range(7):
        j=(i+1)%7;mesh.face([ring[i],top[i],top[j],ring[j]])
        mesh.face([(x,y,z),ring[i],ring[j]])
    for i in range(7):mesh.face([top[i],(x-.08,y+h,z+.04),top[(i+1)%7]])

def crystal(mesh,x,z,r,h):
    low=[(x+cos(i*pi/3)*r,.12,z+sin(i*pi/3)*r) for i in range(6)]
    high=[(a,.12+h*.7,c) for a,_,c in low]
    for i in range(6):
        j=(i+1)%6;mesh.face([low[i],high[i],high[j],low[j]])
        mesh.face([high[i],(x+h*.08,h+.12,z),high[j]])

def build():
    m=shapes.Mesh();log(m,-.3,.3,0,.3,1.3);log(m,.3,.3,.06,.3,1.15);log(m,0,.8,0,.3,1.25)
    m.box(0,.5,0,.08,.95,1.4);m.save('Timber')
    m=shapes.Mesh();boulder(m,-.22,0,0,1.1,1.15,.95);boulder(m,.48,0,-.12,.65,.65,.7);boulder(m,-.5,0,-.35,.55,.43,.6);m.save('Rock')
    m=shapes.Mesh();boulder(m,0,0,0,1.3,.25,.9);crystal(m,0,.08,.3,1.5);crystal(m,-.4,-.12,.22,.9);crystal(m,.4,-.08,.2,1.1);m.save('Crystal')
    print('Generated Timber, Rock and Crystal models in',shapes.OUTPUT)

if __name__=='__main__':build()
