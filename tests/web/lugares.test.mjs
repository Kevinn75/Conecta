import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import lugares from '../../data/lugares.js';
test('514 registros y todos sus campos idénticos a PLACES + EXTRA_PLACES de V5',()=>{
 const v5=readFileSync(new URL('../../docs/referencia/CONECTA_redisenado_experiencia_v5.html',import.meta.url),'utf8');
 const esperado=['PLACES','EXTRA_PLACES'].flatMap(k=>JSON.parse(v5.match(new RegExp('const '+k+'=(\\[.*?\\]);','s'))[1]));
 assert.equal(lugares.length,514);assert.equal(new Set(lugares.map(p=>p.id)).size,514);assert.deepEqual(lugares,esperado);
});
