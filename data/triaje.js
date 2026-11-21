(function(g){'use strict';
const lugares=typeof module!=='undefined'&&module.exports?require('./lugares.js'):g.Lugares;
const catalogo=new Map(lugares.map(p=>[p.id,p]));
const H={'Derrumbe o deslizamiento':'derrumbe','Actividad volcánica':'volcanica','Terremoto o temblor':'terremoto','Personas atrapadas':'atrapadas','Otro peligro / No sé':'otro'};
const N=['Agua potable','Alimentos','Refugio o alojamiento','Apoyo en salud','Otra','No sé todavía'];
const A=['Artículos para bebés','Apoyo para niñas o niños','Apoyo para personas mayores','Ayuda para el traslado'];
const S=['Embarazo o lactancia','Medicamentos de uso continuo','Movilidad reducida','Otra necesidad de salud (sin detalles)'];
const W=['No podemos esperar','Dentro de las próximas 24 horas','Más de 24 horas','No sé'];
const WHO=['Solo a mí','A mi hogar','A varias familias','A otra persona o grupo','No sé'];
const razones={
'Agua potable':['Ya se acabó','Queda poca','Todavía tenemos'],
'Alimentos':['Ya se acabaron','Quedan pocos','Todavía tenemos'],
'Refugio o alojamiento':['No tenemos dónde','Tenemos un lugar inseguro','Sí tenemos dónde'],
'Apoyo en salud':['No puede esperar','Puede esperar algunas horas','Puede esperar más'],
'Otra':['Es urgente','Puede esperar']
};
const genericos=['Es urgente','Se acabó lo que necesitamos','Queda muy poco','Aún tenemos algo','No sé'];
const severos=['No tenemos dónde','Tenemos un lugar inseguro','Ya se acabó','Ya se acabaron','No puede esperar','Puede esperar algunas horas'];
const textos={solicitud:'Autorizo a [Entidad responsable] a usar los datos de esta solicitud para coordinar la atención y la entrega de ayuda.',contacto:'Autorizo a [Entidad responsable] a guardar mi nombre o alias y mi teléfono para que personal autorizado del PMU me contacte sobre este aviso.',salud:'Autorizo a [Entidad responsable] a usar el dato de salud que elija para que el equipo autorizado prepare el apoyo que solicito.'};
const copia=v=>Array.isArray(v)?v.map(copia):v&&typeof v==='object'?Object.fromEntries(Object.entries(v).map(([k,x])=>[k,copia(x)])):v;
const texto=v=>typeof v==='string'?(v.trim()||null):v??null;
const lista=v=>Array.isArray(v)?v.map(texto):[];
const esCantidad=v=>v===null||v===undefined||v===''||v==='No sé';
function cantidad(r){return r.danger==='Sí'?r.rapidPersons:r.unknownPersons?null:r.persons}
function normalizarReporte(raw){
const r=copia(raw&&typeof raw==='object'&&!Array.isArray(raw)?raw:{});
for(const k of ['danger','hazard','topNeed','why','waiting','who','observed','minutes','reference','locationQuery','otherNeed','contactAlias','contactPhone','persons','rapidPersons'])r[k]=texto(r[k]);
for(const k of ['needs','support','healthCats','consents'])r[k]=k==='consents'?(Array.isArray(r[k])?r[k]:[]):lista(r[k]);
for(const k of ['unknownNeighborhood','unknownPersons','reviewConsent','contactConsent','healthConsent'])r[k]=r[k]===true;
const v=cantidad(r);r.personas=esCantidad(v)?null:Number(v);
r.location=r.unknownNeighborhood?null:copia(catalogo.get(r.location?.id)||null);
return r;
}
function contexto(r){const concretas=r.needs.filter(n=>N.includes(n)&&n!=='No sé todavía');const n=concretas.includes(r.topNeed)?r.topNeed:concretas.length===1?concretas[0]:null;const especifico=r.why&&!genericos.includes(r.why);return {n,ambiguo:!!(especifico&&(!n||!razones[n]?.includes(r.why)))}}
function vigente(r,t){const flag=r[{solicitud:'reviewConsent',contacto:'contactConsent',salud:'healthConsent'}[t]];const c=r.consents.filter(c=>c&&c.tipo===t).at(-1);return flag===true&&c?.otorgada===true&&c.versionTexto==='v4-2026-11-20'&&c.textoMostrado===textos[t]&&fechaValida(c.fecha)}
function fechaValida(s){return typeof s==='string'&&/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$/.test(s)&&Number.isFinite(Date.parse(s))&&new Date(s).toISOString()===s}
function validarReporte(raw){
const r=normalizarReporte(raw),errores=[],advertencias=[];
const add=(campo,codigo,mensaje)=>errores.push({campo,codigo,mensaje});
const warn=(campo,codigo,mensaje)=>advertencias.push({campo,codigo,mensaje});
const dominio=(k,opciones)=>{if(r[k]!==null&&!opciones.includes(r[k]))add(k,'dominio','Selecciona una opción válida.')};
dominio('danger',['Sí','No','No sé']);dominio('hazard',Object.keys(H));dominio('waiting',W);dominio('who',WHO);dominio('topNeed',[...N,'Ambas por igual','No sé']);dominio('why',[...new Set(Object.values(razones).flat()),...genericos]);
dominio('observed',['Ahora','Hace aproximadamente','Me lo contaron','No sé']);dominio('minutes',['10 minutos','15 minutos','30 minutos','31 a 60 minutos','Más de una hora']);
if(r.observed==='Hace aproximadamente'&&!r.minutes)add('minutes','coherencia','Indica hace cuánto observaste la situación.');
for(const [k,opciones] of [['needs',N],['support',A],['healthCats',S]]){
if(raw?.[k]!=null&&!Array.isArray(raw[k]))add(k,'tipo','Usa una lista de opciones.');
if(r[k].some(v=>!opciones.includes(v))||new Set(r[k]).size!==r[k].length)add(k,'dominio','Revisa las opciones seleccionadas.');
}
for(const k of ['unknownNeighborhood','unknownPersons','reviewConsent','contactConsent','healthConsent'])if(raw?.[k]!=null&&typeof raw[k]!=='boolean')add(k,'tipo','Usa una respuesta booleana.');
const v=cantidad(r),campo=r.danger==='Sí'?'rapidPersons':'persons';
if(!esCantidad(v)&&(!(typeof v==='number'||typeof v==='string'&&/^\d+$/.test(v))||!Number.isInteger(r.personas)||r.personas<1||r.personas>100000))add(campo,'rango','Indica un entero de 1 a 100000 o No sé.');
if(r.who==='Solo a mí'&&r.personas>1)add('who','coherencia','Solo a mí no coincide con varias personas.');
if(r.needs.includes('No sé todavía')&&r.needs.length>1)add('needs','coherencia','Aclara si conoces las necesidades o todavía no.');
if(r.topNeed&&N.includes(r.topNeed)&&!r.needs.includes(r.topNeed))add('topNeed','coherencia','La prioridad debe ser una necesidad seleccionada.');
if(r.danger!=='Sí'&&r.hazard)warn('hazard','seleccion_abandonada','Se eligió un peligro y luego se cambió la respuesta sobre peligro inmediato');
if(contexto(r).ambiguo)warn('why','contexto','Falta aclarar a qué necesidad corresponde el motivo.');
if(severos.includes(r.why)&&r.waiting==='Más de 24 horas')warn('waiting','contradiccion','Aclara la espera indicada para una necesidad severa.');
if(raw?.location&&!catalogo.has(raw.location.id))add('location','catalogo','Selecciona un lugar del catálogo o describe dónde estás.');
if(raw?.unknownNeighborhood===true&&raw.location)add('location','coherencia','No combines barrio desconocido y lugar seleccionado.');
for(const [k,max] of [['reference',160],['locationQuery',160],['otherNeed',100],['contactAlias',100],['contactPhone',24]])if(r[k]!==null&&(typeof r[k]!=='string'||r[k].length>max))add(k,'longitud','Revisa el tipo o la longitud del texto.');
if(!r.location&&String(r.unknownNeighborhood?r.reference||'':r.locationQuery||r.reference||'').length<6)warn('reference','requerido','Describe el lugar con al menos 6 caracteres.');
if(raw?.consents!=null&&!Array.isArray(raw.consents))add('consents','tipo','El historial debe ser una lista.');
if(r.consents.length>100)add('consents','longitud','El historial supera el límite propuesto.');
let anterior='';
for(const c of r.consents){if(!c||!Object.hasOwn(textos,c.tipo)||typeof c.otorgada!=='boolean'||!fechaValida(c.fecha)||c.versionTexto!=='v4-2026-11-20'||c.textoMostrado!==textos[c.tipo]||c.fecha<anterior){add('consents','historial','Revisa la integridad del historial de autorizaciones.');break}anterior=c.fecha}
for(const [t,k] of [['solicitud','reviewConsent'],['contacto','contactConsent'],['salud','healthConsent']]){const ultimo=r.consents.filter(c=>c?.tipo===t).at(-1);if((r[k]&&!vigente(r,t))||(ultimo&&ultimo.otorgada!==r[k]))add(k,'autorizacion','La autorización debe coincidir con su última decisión explícita.');}
return {errores,advertencias};
}
const motivos={C1:'Se declaró peligro inmediato.',C2:'Se reportaron personas atrapadas.',C3:'Se declaró un evento grave con peligro inmediato para personas.',A1:'Se indicó que la ayuda no puede esperar.',A2:'Se reportó falta de refugio seguro.',A3:'Se reportó agotamiento de un recurso básico.',A4:'Se solicitó atención de salud que requiere coordinación prioritaria.',A5:'No se pudo descartar peligro inmediato.',A6:'Se declaró una necesidad severa cuya atención debe revisarse.',A7:'Faltan datos para descartar urgencia o aclarar el evento declarado.',M1:'No hay indicadores conocidos de prioridad crítica o alta en las respuestas actuales.',V1:'La ubicación necesita verificación.',V2:'Hay información declarada como desconocida.',V3:'Hay datos que deben aclararse sin rebajar la alerta.',V4:'Falta aclarar a qué necesidad corresponde el motivo.',V5:'Faltan respuestas relevantes para interpretar el reporte.',S1:'Se declaró afectación de un grupo; la cantidad no reemplaza las alertas vitales.'};
function clasificarRiesgo(raw){
const r=normalizarReporte(raw),v=validarReporte(raw),urgente=r.danger==='Sí',ctx=contexto(r),reglasActivadas=[],datosFaltantes=[];
if(r.danger===null)datosFaltantes.push('danger');
if(urgente&&!r.hazard)datosFaltantes.push('hazard');
if(!urgente){if(!r.needs.length)datosFaltantes.push('needs');if(r.waiting===null)datosFaltantes.push('waiting');}
if(r.personas===null)datosFaltantes.push(urgente?'rapidPersons':'persons');
const activar=(id,si)=>{if(si)reglasActivadas.push(id)};
activar('C1',urgente);activar('C2',urgente&&r.hazard==='Personas atrapadas');activar('C3',urgente&&['derrumbe','volcanica','terremoto'].includes(H[r.hazard]));
activar('A1',r.waiting==='No podemos esperar');
activar('A2',!ctx.ambiguo&&ctx.n===N[2]&&razones[N[2]].slice(0,2).includes(r.why));
activar('A3',!ctx.ambiguo&&(ctx.n===N[0]&&r.why==='Ya se acabó'||ctx.n===N[1]&&r.why==='Ya se acabaron'));
activar('A4',!ctx.ambiguo&&ctx.n===N[3]&&razones[N[3]].slice(0,2).includes(r.why));
activar('A5',r.danger==='No sé');activar('A6',['Es urgente','Se acabó lo que necesitamos','Queda muy poco'].includes(r.why)||ctx.ambiguo&&severos.includes(r.why));
activar('A7',r.danger===null||!urgente&&(!r.needs.length||r.waiting===null||r.waiting==='No sé'));
const nivel=reglasActivadas.some(x=>x[0]==='C')?'critico':reglasActivadas.length?'alto':'medio';activar('M1',nivel==='medio');
activar('V1',!r.location);activar('V2',r.personas===null||r.why==='No sé'||r.needs.includes('No sé todavía')||urgente&&r.hazard==='Otro peligro / No sé');
const abandonado=!urgente&&!!r.hazard;
activar('V3',abandonado||v.errores.length>0||r.who==='Solo a mí'&&r.personas>1||severos.includes(r.why)&&r.waiting==='Más de 24 horas');
activar('V4',ctx.ambiguo);
activar('V5',datosFaltantes.some(x=>x!=='persons'&&x!=='rapidPersons'));
activar('S1',Number.isInteger(r.personas)&&r.personas>1&&r.personas<=100000||r.who==='A varias familias');
const faltantes=datosFaltantes.filter(k=>!((k==='persons'||k==='rapidPersons')&&(cantidad(r)==='No sé'||!urgente&&r.unknownPersons)));
return {nivel,requiereVerificacion:reglasActivadas.some(x=>x[0]==='V'||x==='A5'||x==='A7'),reglasActivadas,motivos:reglasActivadas.map(x=>x==='V3'&&abandonado?'Se eligió un peligro y luego se cambió la respuesta sobre peligro inmediato':motivos[x]),versionReglas:'triaje-2026-11-20.1',datosFaltantes:faltantes};
}
const opcionalesUrgente=new Set(['rapidPersons','needs','topNeed','why','waiting','who','observed','minutes','otherNeed','persons','unknownPersons','support']);
function erroresBloqueantes(raw,v=validarReporte(raw)){return v.errores.filter(e=>raw?.danger!=='Sí'||!opcionalesUrgente.has(e.campo))}
function construirPayload(raw,triaje){
const r=normalizarReporte(raw),v=validarReporte(raw),urgente=r.danger==='Sí';
if(urgente&&v.errores.some(e=>e.campo==='rapidPersons'))r.personas=null;
v.errores=erroresBloqueantes(raw,v);
const error=(campo,mensaje)=>v.errores.push({campo,codigo:'contrato',mensaje});
if(!/^[A-Za-z0-9_-]{1,64}$/.test(raw?.reportId||''))error('reportId','Falta un identificador válido.');
if(!/^[A-Z0-9]{1,6}$/.test(raw?.torre||''))error('torre','Falta una estación válida.');
if(!fechaValida(raw?.fecha))error('fecha','Falta la fecha del primer intento.');
if(!urgente&&!vigente(r,'solicitud'))error('reviewConsent','Falta la autorización de la solicitud.');
if(!urgente&&!r.needs.length)error('needs','Selecciona una necesidad o No sé todavía.');
if(!r.location&&String(r.unknownNeighborhood?r.reference||'':r.locationQuery||r.reference||'').length<6)error('reference','Describe el lugar con al menos 6 caracteres.');
if(v.errores.length){const e=new Error(v.errores[0].mensaje);e.errores=v.errores;throw e}
const calculado=clasificarRiesgo(raw);
if(triaje&&Object.keys(calculado).some(k=>JSON.stringify(triaje[k])!==JSON.stringify(calculado[k])))throw new Error('El triaje no corresponde a las respuestas actuales.');
const originales={};for(const k of ['danger','hazard','needs','topNeed','why','waiting','unknownNeighborhood','persons','rapidPersons','unknownPersons','who'])originales[k]=copia(raw?.[k]??(k==='needs'?[]:['persons','rapidPersons'].includes(k)?'':['unknownNeighborhood','unknownPersons'].includes(k)?false:null));
const ubicacion=r.location?{id:r.location.id,nombre:r.location.nombre,zona:r.location.zona,municipio:r.location.municipio}:{id:null,texto:r.unknownNeighborhood?r.reference:r.locationQuery||r.reference,zona:null,municipio:null,...(r.unknownNeighborhood?{barrioDesconocido:true}:{})};
const p={idReporte:raw.reportId,torre:raw.torre,ruta:urgente?'urgente':'normal',categoria:urgente?H[r.hazard]||'otro':'necesidades',ubicacion,referencia:r.reference,personas:r.personas,quienIncluye:urgente?null:r.who,observado:urgente?null:r.observed?(r.observed==='Hace aproximadamente'?[r.observed,r.minutes].filter(Boolean).join(' '):r.observed):null,necesidades:urgente?[]:r.needs.slice(),prioridad:{top:urgente?r.hazard:r.topNeed,motivo:urgente?'Peligro inmediato declarado':r.why,espera:urgente?'Revisión inmediata sugerida':r.waiting,revisionSugerida:{critico:'Revisión inmediata sugerida',alto:'Revisión prioritaria sugerida',medio:'Revisión ordinaria sugerida'}[calculado.nivel],razon:calculado.motivos[0],ubicacionPorVerificar:!r.location},apoyos:urgente?[]:r.support.slice(),otraNecesidad:!urgente&&r.needs.includes('Otra')?r.otherNeed:null,autorizaciones:{solicitud:vigente(r,'solicitud'),contacto:vigente(r,'contacto'),salud:vigente(r,'salud')},consents:r.consents.map(c=>({tipo:c.tipo,otorgada:c.otorgada,fecha:c.fecha,versionTexto:c.versionTexto,textoMostrado:c.textoMostrado})),fecha:raw.fecha,versionEsquema:'conecta-reporte-1',triajeSugerido:{...calculado,respuestasOriginales:originales}};
if(vigente(r,'contacto')&&(r.contactAlias||r.contactPhone))p.contacto={alias:r.contactAlias,telefono:r.contactPhone};
if(vigente(r,'salud')&&r.healthCats.length)p.salud=r.healthCats.join(', ');
if(new TextEncoder().encode(JSON.stringify(p)).length>16384)throw new Error('El reporte supera el máximo propuesto de 16 KiB.');
return p;
}
function construirResumenRadio(p){
if(!p||!/^[A-Za-z0-9_-]{1,64}$/.test(p.idReporte||'')||!/^[A-Z0-9]{1,6}$/.test(p.torre||'')||![...Object.values(H),'necesidades'].includes(p.categoria)||!['normal','urgente'].includes(p.ruta)||!Array.isArray(p.necesidades)||p.necesidades.some(n=>!N.includes(n))||p.personas!==null&&(!Number.isInteger(p.personas)||p.personas<1||p.personas>100000))throw new Error('El resumen radio requiere un payload válido.');
const codigos=['AG','AL','RF','SA','OT','NS'];return {id:p.idReporte,t:p.torre,c:p.categoria,u:catalogo.has(p.ubicacion?.id)?p.ubicacion.id:'POR_VERIFICAR',n:[...new Set(p.necesidades.map(n=>codigos[N.indexOf(n)]))],p:p.personas,r:p.ruta==='urgente'?1:0};
}
const api={normalizarReporte,validarReporte,erroresBloqueantes,clasificarRiesgo,construirPayload,construirResumenRadio};g.Triaje=api;if(typeof module!=='undefined'&&module.exports)module.exports=api;
})(globalThis);
