"""Build fixed recipes for Claude phrase generation. Never modifies defaults."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'Generated'
def action(operation='engine_command',**fields):
    return {'operation':operation,'axis':0,'value':0,'name':'','target':'self','targetName':'','reference':'','text':'','duration':1,'data':{},**fields}
def recipe(actions,event='key_pressed',conditions=None,**fields):
    return {'kind':3,'rules':[{'event':event,'key':'Space','interval':1,'tag':'','button':0,'once':False,'variable':'','cooldown':0,'state':'','conditions':conditions or [],'actions':actions,**fields}]}
def main():
    seeds=[];names=set()
    continuous={'controller_move','accelerated_move','waypoint_move','render.layout','text.format'}
    for domain in ('Rendering','Physics','Media','Extended','Spatial'):
        catalog=json.loads((ROOT/f'Docs/ShortText{domain}Commands.json').read_text(encoding='utf-8-sig'))
        for command in catalog['commands']:
            name=command['name']
            if name in names:continue
            names.add(name);event='every_frame' if name in continuous else 'key_pressed'
            seeds.append({'id':name,'description':f"{event}: {command['description']}; exact arguments {json.dumps(command['args'],ensure_ascii=False)}",'requirements':command['requirements'],'recipe':recipe([action(name=name,data=command['args'])],event)})
    def add(id,description,requirements,actions,event='key_pressed',conditions=None,**fields):
        seeds.append(dict(id=id,description=description,requirements=requirements,recipe=recipe(actions,event,conditions,**fields)))
    expr=lambda data:{'test':'expression','axis':0,'value':0,'name':'','data':data}
    add('fade.goal','goalがtrueになったらスプライトの透明度を1秒で0から1にする','SpriteRendererとscene bool goal。0透明、1不透明。',
        [action('tween_property',name='SpriteRenderer.instanceColor_.a',data={'from':0,'to':1},duration=1)],'conditions_entered',[expr({'op':'eq','args':[{'read':'scene:goal'},True]})])
    add('layout.cover','毎フレームScreenBuffer_0を覆う大きさと位置に同期','root単位quadスプライトとScreenBuffer_0へ描くroot等倍無回転Camera2Dが一意。',
        [action(name='render.layout',data={'sourceName':'ScreenBuffer_0','cover':True})],'every_frame')
    for type,name,value in [('vector3','direction',[1,0,0]),('color','tint',[1,0,0,1]),('string','playerName','Player')]:
        add('typed.'+type,f'スペースで変数{name}を{value}へ設定',f'{type}型、playerNameのみglobal、他scene',
            [action(name='variable.set',data={'scope':'global' if type=='string' else 'scene','name':name,'type':type,'value':value})])
    add('global.change','グローバルplayerNameが変わったらHUDへNAME CHANGEDを表示','初回は基準記録、以後の変更で表示',
        [action('set_text',target='name',targetName='HUD',text='NAME CHANGED')],'variable_changed',variable='global:playerName')
    add('condition.range','スペースでhpが1以上10以下なら自分を表示','scene数値hp、両端を含む',
        [action('set_visible',value=1)],conditions=[expr({'op':'between','args':[{'read':'scene:hp'},1,10]})])
    add('condition.or_not','スペースでgoalがtrueまたはhpが0以下でないとき自分を表示','scene変数goal(bool)とhp(number)',
        [action('set_visible',value=1)],conditions=[expr({'op':'or','args':[{'op':'eq','args':[{'read':'scene:goal'},True]},{'op':'not','args':[{'op':'le','args':[{'read':'scene:hp'},0]}]}]})])
    add('condition.component','スペースでAudioSourceがあれば設定済み音を再生','自分に設定済AudioSource',
        [action(name='audio.play',data={'index':0})],conditions=[expr({'op':'contains','args':[{'read':'object:self.components'},'AudioSource']})])
    for target in ('parent','children','descendants','nearest','range','index'):
        data={'radius':5} if target=='range' else {'index':0} if target=='index' else {}
        add('target.'+target,f'スペースでselector={target}の対象を非表示にする','nearest/range/indexはEnemyタグ候補。最寄りは世界座標距離、rangeは5以内、indexは0。それ以外は自分の階層。',
            [action('set_visible',target=target,targetName='Enemy' if target in ('nearest','range','index') else '',value=0,data=data)])
    add('generated.reference','スペースで空Generatedを作ってその生成結果を無効化する','生成直後のID参照',
        [action('create_object',name='Generated'),action('set_active',target='last_created',value=0)])
    add('group.tween','スペースでXスケール2とYスケール3へ同時に1秒で滑らかに補間','Transform、ease_in_out、並列group',
        [action('parallel',data={'branches':[[action('tween_scale',axis=0,value=2,data={'easing':'ease_in_out'})],[action('tween_scale',axis=1,value=3,data={'easing':'ease_in_out'})]]})])
    add('repeat.wait','スペースで0.5秒待ち音を鳴らす処理を3回順番に行う','設定済AudioSource、実行中の再入力は捨てる',
        [action('repeat',data={'count':3,'actions':[action('wait',duration=0.5),action('play_sound')]})])
    add('branch.goal','スペースでgoalがtrueならOPEN、そうでなければCLOSEDを自分へ表示','TextRenderer/BitmapTextRendererとscene bool goal',
        [action('branch',data={'condition':{'op':'eq','args':[{'read':'scene:goal'},True]},'then':[action('set_text',text='OPEN')],'else':[action('set_text',text='CLOSED')]})])
    add('cooldown.sound','スペースで音を鳴らし次の受付まで2秒待つ','設定済AudioSource', [action('play_sound')],cooldown=2)
    add('state.set','スペースで挙動状態をPlayingへ変更','レシピ内状態、状態filterと組合せ',[action('set_state',name='Playing')])
    add('input.window','開始から1秒以上3秒以下の間だけスペースで音を再生','設定済AudioSource、初回更新からの経過時間',
        [action('play_sound')],windowStart=1,windowEnd=3)
    for mode in ('cancel','pause','resume','restart','replace'):
        control=action('sequence_control',data={'rule':0,'mode':mode,**({'actions':[action('play_sound')]} if mode=='replace' else {})})
        program=recipe([action('wait',duration=2),action('play_sound')],'start')
        program['rules']+=recipe([control])['rules']
        seeds.append(dict(id='sequence.'+mode,description=f'開始時2秒待って音を鳴らす。スペースでそのルール0のsequenceを{mode}する。replaceの場合待機を音再生に置き換える。',requirements='設定済AudioSource。resumeはpauseされているときのみ有効。restartは先頭から再開始。',recipe=program))
    OUTPUT.mkdir(exist_ok=True);path=OUTPUT/'ShortTextExtendedSeeds.json'
    path.write_text(json.dumps({'recipes':seeds},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(f'{len(seeds)} fixed recipes: {path}')
if __name__=='__main__':main()
