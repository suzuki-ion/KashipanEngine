"""No network: verify Claude phrase output cannot replace executable seed recipes."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS=Path(__file__).resolve().parents[1]/'Tools'
sys.path.insert(0,str(TOOLS))
import GenerateShortTextExtendedExamples as generator

class Regression(unittest.TestCase):
    def test_rejects_missing_unknown_and_oversized_output(self):
        seed=[{'id':'one'}]
        for payload in ({'items':[]},{'items':[{'id':'unknown','phrases':['a','b','c']}]},
                        {'items':[{'id':'one','phrases':['あ'*171,'b','c']}]},
                        {'items':[{'id':'one','phrases':['a','b']}]},
                        {'items':[{'id':'one','phrases':['a\nb','b','c']}]},
                        {'items':[{'id':'one','phrases':['a','b','c'],'recipe':{}}]}):
            with self.subTest(payload=payload),self.assertRaises(RuntimeError):
                generator.validate_phrases(seed,payload)

    def test_rejects_wrong_model(self):
        with self.assertRaises(RuntimeError):generator.verify_model({'modelUsage':{'another-model':{}}})

    def test_candidate_preserves_seed_and_existing_document(self):
        with tempfile.TemporaryDirectory(prefix='ShortTextGenerator-') as folder:
            root=Path(folder)
            recipe={'kind':3,'rules':[{'event':'start','actions':[{'operation':'engine_command','name':'variable.set','data':{'name':'goal','value':True}}]}]}
            seeds=root/'seeds.json';seeds.write_text(json.dumps({'recipes':[{'id':'one','description':'d','requirements':'r','recipe':recipe}]}),encoding='utf8')
            previous={'version':1,'extra':'preserve','entries':[{'phrase':'existing','mode':'recipe','recipe':{'kind':0}}]}
            production=root/'defaults.json';production.write_text(json.dumps(previous),encoding='utf8')
            reply={'is_error':False,'subtype':'success','modelUsage':{'claude-opus-5-5':{}},'structured_output':{'items':[{'id':'one','phrases':['第一例','第二例','第三例']}]},'total_cost_usd':0.01,'duration_ms':10}
            completed=subprocess.CompletedProcess([],0,json.dumps(reply),'')
            values={'PRODUCTION':production,'CANDIDATE':root/'candidate.json','REPORT':root/'report.json','RAW_DIR':root/'raw'}
            with patch.multiple(generator,**values),patch.object(generator,'resolve_claude',return_value='unused'),patch.object(generator,'run_claude',return_value=(completed,0.01)),patch.object(sys,'argv',['generator','--seeds',str(seeds)]):
                self.assertEqual(generator.main(),0)
            candidate=json.loads((root/'candidate.json').read_text(encoding='utf8'))
            self.assertEqual(candidate['extra'],'preserve')
            self.assertEqual(candidate['entries'][0],previous['entries'][0])
            for entry in candidate['entries'][1:]:self.assertEqual(entry['recipe'],recipe)
            self.assertEqual(json.loads(production.read_text(encoding='utf8')),previous)

if __name__=='__main__':unittest.main()
