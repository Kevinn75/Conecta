import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import T from '../../data/triaje.js';
import lugares from '../../data/lugares.js';
const lugar=lugares[0];
const base=()=>({danger:'No',hazard:null,needs:['Agua potable'],topNeed:null,why:null,waiting:'Más de 24 horas',location:{...lugar},unknownNeighborhood:false,persons:'1',rapidPersons:'1',unknownPersons:false,who:null});
const urgente={danger:'Sí',hazard:'Personas atrapadas',needs:[],waiting:null};
// Casos documentales completos: no representan validación clínica ni de radio.
const casos=[
 ['ordinario',{},'medio',false,['M1']],
 ['peligro vital individual',urgente,'critico',false,['C1','C2']],
 ['deslizamiento y varios hogares',{...urgente,hazard:'Derrumbe o deslizamiento',rapidPersons:'8',who:'A varias familias'},'critico',false,['C1','C3','S1']],
 ['alerta vital sin ubicación',{...urgente,location:null,unknownNeighborhood:true,reference:'Cancha ficticia'},'critico',true,['C1','C2','V1']],
 ['urgente sin ubicación',{waiting:'No podemos esperar',location:null,reference:'Cancha ficticia'},'alto',true,['A1','V1']],
 ['No sé sobre peligro',{danger:'No sé'},'alto',true,['A5']],
 ['datos incompletos',{danger:null,needs:[],waiting:null,persons:null,location:null},'alto',true,['A7','V1','V2','V5']],
 ['necesidades múltiples contextualizadas',{needs:['Agua potable','Alimentos'],topNeed:'Agua potable',why:'Ya se acabó',waiting:'Dentro de las próximas 24 horas'},'alto',false,['A3']],
 ['sin refugio',{needs:['Refugio o alojamiento'],why:'No tenemos dónde',waiting:'Dentro de las próximas 24 horas'},'alto',false,['A2']],
 ['refugio inseguro',{needs:['Refugio o alojamiento'],why:'Tenemos un lugar inseguro',waiting:'No podemos esperar'},'alto',false,['A1','A2']],
 ['salud no puede esperar',{needs:['Apoyo en salud'],why:'No puede esperar',waiting:'No podemos esperar'},'alto',false,['A1','A4']],
 ['salud algunas horas',{needs:['Apoyo en salud'],why:'Puede esperar algunas horas',waiting:'Dentro de las próximas 24 horas'},'alto',false,['A4']],
 ['24 horas sin otra señal',{waiting:'Dentro de las próximas 24 horas'},'medio',false,['M1']],
 ['espera desconocida',{waiting:'No sé'},'alto',true,['A7']],
 ['caso 15: atrapadas abandonado',{hazard:'Personas atrapadas'},'medio',true,['M1','V3']],
 ['caso 16: deslizamiento abandonado',{hazard:'Derrumbe o deslizamiento'},'medio',true,['M1','V3']],
 ['peligro desconocido y cantidad ausente',{...urgente,hazard:'Otro peligro / No sé',rapidPersons:null},'critico',true,['C1','V2']],
 ['grupo numeroso sin alerta vital',{persons:'100',who:'A varias familias'},'medio',false,['M1','S1']],
 ['necesidad y cantidad desconocidas',{needs:['No sé todavía'],persons:null},'medio',true,['M1','V2']],
 ['motivo No sé',{why:'No sé'},'medio',true,['M1','V2']],
 ['motivo ambiguo entre necesidades',{needs:['Agua potable','Refugio o alojamiento'],topNeed:'Ambas por igual',why:'No tenemos dónde',waiting:'Dentro de las próximas 24 horas'},'alto',true,['A6','V4']],
 ['contradicción de espera',{why:'Ya se acabó'},'alto',true,['A3','V3']],
 ['peligro sin categoría',{...urgente,hazard:null},'critico',true,['C1','V5']],
 ['personas negativas',{persons:'-2'},'medio',true,['M1','V3']],
 ['peligro vital con cantidad inválida',{...urgente,rapidPersons:'-2'},'critico',true,['C1','C2','V3']],
 ['corregido No sé a No',{danger:'No'},'medio',false,['M1']],
 ['corregido No a Sí',urgente,'critico',false,['C1','C2']],
 ['texto manual sin catálogo',{location:null,locationQuery:'Lugar ficticio'},'medio',true,['M1','V1']],
 ['Solo a mí contradictorio',{who:'Solo a mí',persons:'4'},'medio',true,['M1','V3','S1']],
 ['actividad volcánica',{...urgente,hazard:'Actividad volcánica'},'critico',false,['C1','C3']],
 ['terremoto',{...urgente,hazard:'Terremoto o temblor'},'critico',false,['C1','C3']]
];
for(const [nombre,cambios,nivel,verificacion,reglas] of casos)test(nombre,()=>{
 const raw={...base(),...cambios},antes=structuredClone(raw),t=T.clasificarRiesgo(raw);
 assert.equal(t.nivel,nivel);assert.equal(t.requiereVerificacion,verificacion);assert.deepEqual(t.reglasActivadas,reglas);
 assert.equal(t.motivos.length,reglas.length);assert.equal(t.versionReglas,'triaje-2026-11-20.1');assert.deepEqual(raw,antes);
 assert.deepEqual(T.clasificarRiesgo(raw),t);
});
test('cambio de respuesta antes de enviar no acumula reglas obsoletas',()=>{
 const raw={...base(),...urgente};assert.equal(T.clasificarRiesgo(raw).nivel,'critico');
 Object.assign(raw,{danger:'No',needs:['Agua potable'],waiting:'Más de 24 horas'});
 const t=T.clasificarRiesgo(raw);assert.deepEqual(t.reglasActivadas,['M1','V3']);assert.equal(t.motivos[1],'Se eligió un peligro y luego se cambió la respuesta sobre peligro inmediato');
 raw.danger='No sé';assert.deepEqual(T.clasificarRiesgo(raw).reglasActivadas,['A5','V3']);
 raw.hazard='Otro peligro / No sé';raw.danger='No';assert.deepEqual(T.clasificarRiesgo(raw).reglasActivadas,['M1','V3']);
});
test('sospecha de autenticidad no suprime personas atrapadas',()=>{
 const t=T.clasificarRiesgo({...base(),...urgente,revisionAntifraude:true});assert.equal(t.nivel,'critico');assert.ok(t.reglasActivadas.includes('C2'));
});
test('normalización conserva original, distingue cero, null y No sé',()=>{
 const raw={...base(),danger:' No ',persons:'0',needs:[' Agua potable ']};const antes=structuredClone(raw);
 const n=T.normalizarReporte(raw);assert.equal(n.danger,'No');assert.equal(n.personas,0);n.needs.push('Otra');n.location.nombre='alterado';assert.deepEqual(raw,antes);
 for(const persons of [null,'','No sé'])assert.equal(T.normalizarReporte({...base(),persons}).personas,null);
 assert.ok(!T.clasificarRiesgo({...base(),persons:'No sé'}).datosFaltantes.includes('persons'));
});
test('rango de personas 1–100000 en ambas rutas',()=>{
 for(const danger of ['Sí','No'])for(const valor of [-1,0,1.5,100001,'texto','Infinity',true,{},'-1','0','1.2','100001']){
  const campo=danger==='Sí'?'rapidPersons':'persons';const v=T.validarReporte({...base(),danger,[campo]:valor});assert.ok(v.errores.some(e=>e.campo===campo),`${danger}: ${String(valor)}`);
 }
 for(const persons of ['1','100000',1,100000,null,'No sé'])assert.deepEqual(T.validarReporte({...base(),persons}).errores,[]);
});
test('combinaciones incoherentes y tipos no se aceptan',()=>{
 for(const cambio of [{who:'Solo a mí',persons:'2'},{needs:['No sé todavía','Agua potable']},{topNeed:'Alimentos'},{needs:'Agua potable'},{needs:['Agua potable','Agua potable']},{danger:true},{unknownPersons:'false'},{location:{id:'inventado'}}])assert.ok(T.validarReporte({...base(),...cambio}).errores.length);
});
const fuente=readFileSync(new URL('../../docs/referencia/CONECTA_redisenado_experiencia_v5.html',import.meta.url),'utf8');
const consent=(tipo,otorgada=true,fecha='2026-11-20T15:00:00.000Z')=>({tipo,otorgada,fecha,versionTexto:'v4-2026-11-20',textoMostrado:fuente.match(new RegExp(tipo+":'([^']+)'"))[1]});
const listo=()=>({...base(),...urgente,reportId:'FICTICIO-R001',torre:'E01',fecha:'2026-11-20T15:00:00.000Z'});
test('payload con originales y sin credenciales; radio con lista cerrada',()=>{
 const raw={...listo(),privateKey:'SECRETO',hashClave:'HASH',keyHash:'HASH',contactConsent:true,contactAlias:'ALIAS_FICTICIO',contactPhone:'TELEFONO_FICTICIO',healthConsent:true,healthCats:['Movilidad reducida'],consents:[consent('contacto'),consent('salud')],reference:'REFERENCIA_FICTICIA'};
 const p=T.construirPayload(raw,T.clasificarRiesgo(raw));assert.equal(p.contacto.alias,'ALIAS_FICTICIO');assert.equal(p.salud,'Movilidad reducida');assert.equal(p.triajeSugerido.respuestasOriginales.danger,'Sí');
 assert.ok(!/SECRETO|HASH|hashClave|privateKey|keyHash/.test(JSON.stringify(p)));
 const radio=T.construirResumenRadio(p);assert.deepEqual(Object.keys(radio),['id','t','c','u','n','p','r']);assert.ok(!/ALIAS|TELEFONO|Movilidad|REFERENCIA|contacto|salud/.test(JSON.stringify(radio)));
 assert.equal(T.construirResumenRadio({...p,ubicacion:{id:null,zona:'TEXTO_LIBRE'}}).u,'POR_VERIFICAR');
});
test('revocación omite contacto y salud; historial literal se conserva',()=>{
 const raw={...listo(),contactAlias:'No enviar',healthCats:['Movilidad reducida'],contactConsent:false,healthConsent:false,consents:[consent('contacto'),consent('salud'),consent('contacto',false),consent('salud',false)]};
 const p=T.construirPayload(raw);assert.equal('contacto' in p,false);assert.equal('salud' in p,false);assert.deepEqual(p.consents,raw.consents);
 assert.throws(()=>T.construirPayload({...raw,contactConsent:true}),/autorización/);
 assert.throws(()=>T.construirPayload({...raw,consents:[{...consent('contacto'),textoMostrado:'alterado'}]}));
});
test('autorización normal, referencia desconocida e identidad son obligatorias en payload',()=>{
 assert.throws(()=>T.construirPayload({...listo(),danger:'No'}));
 const raw={...listo(),danger:'No',needs:['Agua potable'],waiting:'No sé',reviewConsent:true,consents:[consent('solicitud')]};assert.equal(T.construirPayload(raw).ruta,'normal');
 assert.throws(()=>T.construirPayload({...listo(),unknownNeighborhood:true,location:null,reference:'abc'}));
 assert.throws(()=>T.construirPayload({...listo(),reportId:'a/b'}));
 assert.throws(()=>T.construirPayload({...listo(),torre:'<E01>'}));
 assert.throws(()=>T.construirPayload({...listo(),fecha:null}));
 assert.throws(()=>T.construirPayload(listo(),T.clasificarRiesgo(base())));
});

for(const valor of ['0','-3','1.5','200000','texto'])test(`H2: urgente con cifra ${valor} conserva evidencia y envía null`,()=>{
 const raw={...listo(),rapidPersons:valor},antes=structuredClone(raw);
 assert.ok(T.validarReporte(raw).errores.some(e=>e.campo==='rapidPersons'));
 const p=T.construirPayload(raw,T.clasificarRiesgo(raw));
 assert.equal(p.personas,null);assert.equal(p.triajeSugerido.nivel,'critico');
 assert.ok(p.triajeSugerido.reglasActivadas.includes('V3'));
 assert.equal(p.triajeSugerido.respuestasOriginales.rapidPersons,valor);
 assert.equal(T.construirResumenRadio(p).p,null);assert.deepEqual(raw,antes);
 const normal={...raw,danger:'No',persons:valor,needs:['Agua potable'],reviewConsent:true,consents:[consent('solicitud')]};
 assert.throws(()=>T.construirPayload(normal),/entero/);
});

test('H2: residuos normales no bloquean el aviso urgente ni pasan a sus campos operativos',()=>{
 const raw={...listo(),rapidPersons:'4',who:'Solo a mí',observed:'Hace aproximadamente',minutes:null,needs:['No sé todavía','Agua potable'],topNeed:'Alimentos',support:['opción inválida'],otherNeed:'x'.repeat(101)};
 const antes=structuredClone(raw),p=T.construirPayload(raw);
 assert.equal(p.personas,4);assert.equal(p.quienIncluye,null);assert.equal(p.observado,null);
 assert.equal(p.otraNecesidad,null);assert.deepEqual(p.necesidades,[]);assert.deepEqual(p.apoyos,[]);
 assert.equal(p.triajeSugerido.nivel,'critico');assert.ok(p.triajeSugerido.reglasActivadas.includes('V3'));
 assert.deepEqual(raw,antes);
 assert.throws(()=>T.construirPayload({...raw,danger:'No',reviewConsent:true,consents:[consent('solicitud')]}));
});
