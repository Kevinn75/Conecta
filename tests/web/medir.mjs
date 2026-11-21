import {readdirSync,readFileSync,statSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {join} from 'node:path';
const dir=fileURLToPath(new URL('../../data/',import.meta.url));
const permitidos=new Set(['index.html','estilo.css','app.js','lugares.js','triaje.js','Data.sql']);
let total=0,fallos=[];
for(const nombre of readdirSync(dir).sort()){
 const ruta=join(dir,nombre),stat=statSync(ruta);
 if(!permitidos.delete(nombre)||!stat.isFile()){fallos.push('Archivo no permitido: '+nombre);continue}
 console.log(`${nombre}: ${stat.size} bytes`);if(nombre==='Data.sql')continue;
 total+=stat.size;
 const texto=readFileSync(ruta,'utf8').replace(/xmlns\s*=\s*(["'])http:\/\/www\.w3\.org\/2000\/svg\1/g,'');
 if(/https?:\/\//i.test(texto))fallos.push(nombre+': URL http(s) fuera de xmlns SVG');
 if(/localhost|192\.168\./i.test(texto))fallos.push(nombre+': host o IP fija');
 if(/fetch\s*\(\s*['"`]\s*(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(texto))fallos.push(nombre+': fetch con URL absoluta');
}
console.log(`Total data/ sin Data.sql: ${total} bytes; objetivo < 100000: ${total<100000?'cumplido':'no cumplido'}`);
for(const nombre of permitidos)fallos.push('Falta '+nombre);
if(fallos.length){console.error(fallos.join('\n'));process.exitCode=1}
