"""Generate 16 road connection variants with raised curbs and lane markings."""
from GenerateTownModels import Mesh
from pathlib import Path
OUT=Path(__file__).resolve().parents[1]/'Assets/Application/Models/Town/Roads'

def build():
    OUT.mkdir(parents=True,exist_ok=True)
    edges=[-.5,-.3,.3,.5]
    for mask in range(16):
        occupied={(1,1)}
        for bit,cell in [(1,(0,1)),(2,(2,1)),(4,(1,0)),(8,(1,2))]:
            if mask&bit:occupied.add(cell)
        asphalt,details=Mesh(),Mesh()
        for x,z in sorted(occupied):
            x0,x1=edges[x:x+2];z0,z1=edges[z:z+2]
            asphalt.box((x0+x1)/2,.025,(z0+z1)/2,x1-x0,.05,z1-z0)
            for dx,dz in [(-1,0),(1,0),(0,-1),(0,1)]:
                if (x+dx,z+dz) in occupied:continue
                # 開いた端は、継ぎ目なく隣接セルへつながる。
                if x+dx<0 or x+dx>2 or z+dz<0 or z+dz>2:continue
                if dx:details.box(x0 if dx<0 else x1,.065,(z0+z1)/2,.035,.09,z1-z0)
                else:details.box((x0+x1)/2,.065,z0 if dz<0 else z1,x1-x0,.09,.035)
        for bit,x,z,wx,wz in [(1,-.4,0,.13,.025),(2,.4,0,.13,.025),(4,0,-.4,.025,.13),(8,0,.4,.025,.13)]:
            if mask&bit:details.box(x,.052,z,wx,.004,wz)
        for label,mesh in [('Surface',asphalt),('Details',details)]:
            # エンジンでの Assimp の座標変換は Z 軸を反転させる。グリッドの接続マスクを保つため、
            # 生成する頂点と法線を反転し、頂点の回り順も逆にする。
            lines=[]
            for line in mesh.lines:
                parts=line.split()
                if parts and parts[0] in ('v','vn'):parts[3]=str(-float(parts[3]));line=' '.join(parts)
                elif parts and parts[0]=='f':line='f '+' '.join(reversed(parts[1:]))
                lines.append(line)
            (OUT/f'Road{mask:02d}{label}.obj').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print('Generated 32 OBJ meshes for all 16 road masks.')
if __name__=='__main__':build()
