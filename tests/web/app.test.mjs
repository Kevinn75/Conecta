import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import ConectaEnvio from '../../data/app.js';
import Triaje from '../../data/triaje.js';
import Lugares from '../../data/lugares.js';
const raw=()=>({danger:'Sí',hazard:'Personas atrapadas',rapidPersons:'1',needs:[],location:Lugares[0],torre:'E01'});
const res=(status,body)=>({status,json:async()=>body});
const confirmado=(status,init)=>res(status,{estado:'guardado',recibidoEnEstacion:true,idReporte:JSON.parse(init.body).idReporte});
test('demo por defecto no hace POST; estación solo con parámetro explícito',async()=>{
 for(const search of ['', '?modo=real','?estacion=true','?modo=demo']){let n=0;const e=ConectaEnvio.crear({search,fetch:()=>{n++;throw Error()}});assert.equal(e.modo,'demo');assert.equal((await e.enviar(raw())).estado,'sin_confirmar');assert.equal(n,0)}
 assert.equal(ConectaEnvio.crear({search:'?modo=estacion'}).modo,'estacion');
});
test('200 en_espera es sin_confirmar',async()=>{
 const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async()=>res(200,{status:'en_espera'})});const r=await e.enviar(raw());assert.equal(r.estado,'sin_confirmar');assert.match(r.mensaje,/sin confirmar/);
});
test('solo 200/201 con guardado, booleano true y mismo ID confirman',async()=>{
 for(const status of [200,201]){const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async(url,init)=>{assert.equal(url,'/api/reporte');assert.equal(init.method,'POST');assert.equal(init.headers['Content-Type'],'application/json');assert.ok(init.signal instanceof AbortSignal);return confirmado(status,init)}});assert.equal((await e.enviar(raw())).estado,'guardado')}
 for(const [status,body] of [[200,{}],[200,null],[200,{estado:'guardado',recibidoEnEstacion:true,idReporte:'otro'}],[201,{estado:'guardado',recibidoEnEstacion:'true'}],[202,{estado:'guardado',recibidoEnEstacion:true}],[204,{}],[299,{}]]){
  const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async()=>res(status,body)});assert.equal((await e.enviar(raw())).estado,'sin_confirmar');
 }
});
test('corte tras POST: reintento conserva ID, fecha y todo el cuerpo',async()=>{
 const envios=[];const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async(url,init)=>{envios.push(init.body);if(envios.length===1)throw new TypeError('corte');return confirmado(200,init)}});
 const original=raw();const r=await e.enviar(original);assert.equal(r.estado,'sin_confirmar');original.rapidPersons='100';original.hazard='Terremoto o temblor';
 assert.equal((await e.enviar(original)).estado,'guardado');assert.equal(envios[0],envios[1]);assert.equal(JSON.parse(envios[0]).idReporte,r.idReporte);
});
test('doble clic comparte intento y hace un único POST',async()=>{
 let liberar,n=0;const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:(url,init)=>{n++;return new Promise(resolve=>{liberar=()=>resolve(confirmado(201,init))})}});
 const a=e.enviar(raw()),b=e.enviar(raw());assert.equal(a,b);assert.equal(n,1);assert.equal(e.enCurso,true);liberar();assert.equal((await a).estado,'guardado');assert.equal(e.enCurso,false);
});
test('JSON inválido conserva estado sin confirmar e identidad',async()=>{
 const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async()=>({status:200,json:async()=>{throw new SyntaxError('JSON')}})});const a=await e.enviar(raw()),b=await e.enviar(raw());assert.equal(a.codigo,'json_invalido');assert.equal(a.estado,'sin_confirmar');assert.equal(a.idReporte,b.idReporte);
});
for(const status of [400,409,422,429,503])test(`HTTP ${status}: error explícito sin confirmar`,async()=>{
 const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async()=>res(status,{estado:'guardado',recibidoEnEstacion:true})});const r=await e.enviar(raw());assert.equal(r.estado,'sin_confirmar');assert.equal(r.http,status);assert.equal(r.codigo,'http_'+status);
});
test('revocación impide reenviar instantánea con datos anteriores',async()=>{
 let n=0;const e=ConectaEnvio.crear({search:'?modo=estacion',fetch:async()=>{n++;throw Error('corte')}});await e.enviar(raw());e.impedirReintento();const r=await e.enviar(raw());assert.equal(n,1);assert.equal(r.codigo,'autorizacion');assert.equal(e.reintentoBloqueado,true);
});
const fuente=readFileSync(new URL('../../data/app.js',import.meta.url),'utf8');
function entorno(extra={}){return {Triaje,Lugares,URLSearchParams,AbortController,TextEncoder,setTimeout,clearTimeout,console,location:{search:''},...extra}}
test('funciona con y sin crypto.subtle; no calcula ni transporta hashes',async()=>{
 for(const crypto of [undefined,{}, {subtle:{digest(){throw Error('No debe calcular hashes')}},getRandomValues(a){a.fill(42);return a}}]){
  let cuerpo;const sandbox=entorno({crypto});vm.runInNewContext(fuente,sandbox);
  const e=sandbox.ConectaEnvio.crear({search:'?modo=estacion',fetch:async(url,init)=>{cuerpo=init.body;return confirmado(201,init)}});
  assert.equal((await e.enviar({...raw(),privateKey:'SECRETO',hashClave:'HASH',keyHash:'HASH'})).estado,'guardado');assert.ok(!/SECRETO|HASH|hashClave|privateKey|keyHash/.test(cuerpo));
 }
});
test('timeout de 15 s aborta y no confirma',async()=>{
 let agotar,espera,limpio=false;const sandbox=entorno({setTimeout(fn,ms){agotar=fn;espera=ms;return 7},clearTimeout(id){assert.equal(id,7);limpio=true}});vm.runInNewContext(fuente,sandbox);
 const e=sandbox.ConectaEnvio.crear({search:'?modo=estacion',fetch:(url,init)=>new Promise((resolve,reject)=>init.signal.addEventListener('abort',()=>reject(Object.assign(Error('aborto'),{name:'AbortError'}))))});
 const pendiente=e.enviar(raw());assert.equal(espera,15000);agotar();assert.equal((await pendiente).codigo,'timeout');assert.equal(limpio,true);
});
// Dobles mínimos del DOM para lógica y foco; no sustituyen revisión visual/VoiceOver.
function interfaz(search=''){
 const nodos=new Map(),focos=[];
 function nodo(id){if(!nodos.has(id))nodos.set(id,{id,innerHTML:'',textContent:'',value:'',classList:{toggle(){},add(){},contains(){return false}},addEventListener(){},setAttribute(){},focus(){focos.push(id)},querySelector(sel){return sel==='h2'?{focus(){focos.push('h2')}}:null},querySelectorAll(){return []}});return nodos.get(id)}
 const document={getElementById:nodo,querySelector(){return nodo('phone')},addEventListener(){}};
 const sandbox=entorno({document,location:{search},window:{scrollTo(){},scrollY:0}});
 const instrumentada=fuente.replace(/\}\s*$/,`globalThis.ui={state,initialState,route,goto,dispatch,validLocation,filterPlaces,logConsent,page,render,next,updateField,clickHandler};\n}`);
 vm.runInNewContext(instrumentada,sandbox);return {ui:sandbox.ui,nodos,focos,sandbox};
}
test('rutas V5, respuestas vacías y foco al cambiar de pantalla',()=>{
 const {ui,focos}=interfaz();assert.deepEqual(focos,[]);assert.equal(ui.state.danger,null);assert.equal(ui.state.persons,'');assert.equal(ui.state.reviewConsent,false);assert.equal(ui.state.contactConsent,false);assert.equal(ui.state.healthConsent,false);
 assert.equal(JSON.stringify(ui.route()),JSON.stringify(['inicio','ubicacion','necesidades','espera','revision','comprobante']));ui.state.danger='Sí';assert.equal(JSON.stringify(ui.route()),JSON.stringify(['inicio','urgencia','comprobante']));ui.goto('urgencia');assert.equal(focos.at(-1),'h2');
});
test('No sé en qué barrio estoy exige referencia y cantidades enteras',()=>{
 const {ui,nodos}=interfaz();ui.goto('ubicacion');ui.dispatch('toggleUnknownNeighborhood');assert.equal(ui.state.unknownNeighborhood,true);assert.equal(ui.validLocation(),false);assert.match(nodos.get('inlineMessage').innerHTML,/role="alert"/);ui.state.reference='Cancha ficticia';ui.state.unknownPersons=true;assert.equal(ui.validLocation(),true);
 ui.state.unknownPersons=false;ui.state.persons='1.5';assert.equal(ui.validLocation(),false);ui.state.persons='100000';assert.equal(ui.validLocation(),true);
});
test('buscador conserva tildes ignoradas y aliases del catálogo',()=>{
 const {ui}=interfaz();assert.equal(JSON.stringify(ui.filterPlaces('Milán')),JSON.stringify(ui.filterPlaces('Milan')));
 const p=Lugares.find(p=>p.aliases?.length);assert.ok(ui.filterPlaces(p.aliases[0]).some(x=>x.id===p.id));
});
test('historial y revocación locales; pantalla legal conserva etiquetas accesibles',()=>{
 const {ui}=interfaz();ui.state.reportId='DEMO-LOCAL';ui.state.contactConsent=true;ui.logConsent('contacto',true);ui.state.contactAlias='Ficticio';ui.dispatch('revokeContact');assert.equal(ui.state.contactAlias,'');assert.equal(ui.state.consents.at(-1).otorgada,false);
 ui.goto('autorizaciones');assert.match(ui.page(),/for="authCode"/);assert.match(ui.page(),/for="authSecret"/);ui.state.verified=true;ui.dispatch('toggleHistory');assert.match(ui.page(),/Autorizo a \[Entidad responsable\]/);
 ui.dispatch('reqCorreccion');ui.state.reqText='Corrección ficticia';ui.dispatch('sendRequest');assert.equal(ui.state.requests.length,1);
});
test('estación no autentica con PIN local ni finge recepción de solicitudes',()=>{
 const {ui}=interfaz('?modo=estacion');ui.state.reportId='DEMO-LOCAL';ui.state.privateKey='123456';ui.state.authCode=ui.state.reportId;ui.state.authSecret=ui.state.privateKey;ui.dispatch('verifyAuth');assert.equal(ui.state.verified,false);
 ui.state.reqMode='correccion';ui.state.reqText='Ficticio';ui.dispatch('sendRequest');assert.equal(ui.state.requests.length,0);
});
test('recursos relativos en HTML, sin scripts inline ni puente PMU',()=>{
 const html=readFileSync(new URL('../../data/index.html',import.meta.url),'utf8');for(const nombre of ['estilo.css','lugares.js','triaje.js','app.js'])assert.ok(html.includes('"'+nombre+'"'));
 assert.ok(!/<script>/.test(html));assert.ok(!/BroadcastChannel|Promise\.resolve|setInterval|onPMU/.test(fuente));
});

function elegirPeligro(ui,value){ui.clickHandler({target:{closest(selector){return selector==='[data-choice]'?{dataset:{choice:'danger',value,check:'false'}}:null}}})}
const snapshot=s=>JSON.parse(JSON.stringify(s));
for(const [desde,hacia] of [[null,'No'],[null,'No sé'],[null,'Sí'],['Sí','No'],['Sí','No sé'],['No','Sí'],['No sé','Sí']])test(`H1: ${desde} → ${hacia} limpia solo la otra ruta`,()=>{
 const {ui}=interfaz();Object.assign(ui.state,{danger:desde,hazard:'Personas atrapadas',rapidPersons:'7',needs:['Alimentos'],topNeed:'Alimentos',why:'Ya se acabaron',whyFor:'Alimentos',waiting:'No podemos esperar',who:'Solo a mí',observed:'Hace aproximadamente',minutes:'10 minutos',otherNeed:'Dato ficticio',persons:'4',unknownPersons:true,support:['Ayuda para el traslado'],location:Lugares[0],locationQuery:Lugares[0].nombre,reference:'Referencia ficticia',unknownNeighborhood:true,reviewConsent:true,contactConsent:true,contactAlias:'Alias ficticio',contactPhone:'3000000000',healthConsent:true,healthCats:['Movilidad reducida'],consents:[{tipo:'contacto',otorgada:true}],requests:[{tipo:'correccion',texto:'Ficticio'}],verified:true,authCode:'FICTICIO'});
 const esperado=snapshot(ui.state);
 Object.assign(esperado,hacia==='Sí'?{needs:[],topNeed:null,why:null,whyFor:null,waiting:null,who:null,observed:null,minutes:null,otherNeed:'',persons:'',unknownPersons:false,support:[]}:{hazard:null,rapidPersons:''},{danger:hacia});
 elegirPeligro(ui,hacia);assert.deepEqual(snapshot(ui.state),esperado);
});
test('H1: repetir respuesta o cambiar entre No y No sé conserva datos de la misma ruta',()=>{
 for(const [desde,hacia] of [['Sí','Sí'],['No','No'],['No','No sé'],['No sé','No']]){
  const {ui}=interfaz();Object.assign(ui.state,{danger:desde,hazard:'Personas atrapadas',rapidPersons:'3',needs:['Alimentos'],persons:'2'});
  const esperado={...snapshot(ui.state),danger:hacia};elegirPeligro(ui,hacia);assert.deepEqual(snapshot(ui.state),esperado);
 }
});
for(const valor of ['0','-3','1.5','200000','texto'])test(`H2: UI crea y envía el aviso con cifra ${valor} y advertencia`,async()=>{
 const {ui,nodos,sandbox}=interfaz('?modo=estacion');const enviados=[];
 sandbox.fetch=async(url,init)=>{enviados.push(JSON.parse(init.body));return res(200,{status:'en_espera'})};
 Object.assign(ui.state,raw(),{locationQuery:Lugares[0].nombre,rapidPersons:valor,who:'Solo a mí',persons:'4'});ui.goto('urgencia');
 ui.dispatch('sendRapid');await new Promise(resolve=>setImmediate(resolve));
 assert.equal(ui.state.screen,'comprobante');assert.ok(ui.state.reportId);assert.equal(enviados.length,1);
 assert.equal(enviados[0].personas,null);assert.ok(enviados[0].triajeSugerido.reglasActivadas.includes('V3'));
 assert.match(nodos.get('app').innerHTML,/cantidad de personas no es válida/);
 assert.match(nodos.get('app').innerHTML,/role="status" aria-live="polite"/);
});
test('H2: UI normal sigue bloqueando una cifra inválida',()=>{
 const {ui}=interfaz();Object.assign(ui.state,{danger:'No',persons:'0',location:Lugares[0],locationQuery:Lugares[0].nombre,needs:['Alimentos'],waiting:'Más de 24 horas',reviewConsent:true});
 ui.logConsent('solicitud',true);ui.goto('revision');ui.dispatch('sendNormal');
 assert.equal(ui.state.screen,'revision');assert.equal(ui.state.reportId,'');assert.match(ui.state.statusMessage,/entero/);
});
