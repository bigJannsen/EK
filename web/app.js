const state = { databases: [], currentDb: null, entries: [], list: [] };

const formatEuroFromCent = v => (v / 100).toFixed(2).replace('.', ',');
const toCentFromEuro = v => {
    const n = parseFloat(String(v).replace(',', '.'));
    return Number.isNaN(n) ? null : Math.round(n * 100);
};

const formatMenge = (wert, einheit) => {
    const n = Number(wert);
    let wertText;
    if (Number.isFinite(n)) {
        wertText = Math.abs(n - Math.round(n)) < 1e-6 ? String(Math.round(n)) : n.toString().replace('.', ',');
    } else {
        wertText = wert != null ? String(wert) : '';
    }
    const einheitText = einheit === 'stk' ? 'Stk' : einheit === 'l' ? 'L' : einheit === 'kg' ? 'Kg' : (einheit || '');
    return einheitText ? `${wertText} ${einheitText}`.trim() : wertText;
};

// läuft, keine ahnung wie aber nicht anfassen !!!!!
async function apiFetch(path, opt = {}) {
    const r = await fetch(path, opt), t = r.headers.get('content-type') || '';
    if (!r.ok) {
        const e = t.includes('json') ? await r.json().catch(() => ({})) : {};
        throw new Error(e.error || `Fehler ${r.status}`);
    }
    return t.includes('json') ? r.json() : r.text();
}

const el = id => document.getElementById(id);

const createFormBody = p => {
    const b = new URLSearchParams();
    for (const [k, v] of Object.entries(p)) if (v != null) b.append(k, v);
    return b;
};

function fillDatabaseSelect() {
    const s = el('database-select');
    s.innerHTML = state.databases.map(n => `<option>${n}</option>`).join('');
    if (state.currentDb) s.value = state.currentDb;
}

function renderDatabaseTable() {
    const tbody = document.querySelector('#database-table tbody');
    tbody.innerHTML = '';
    state.entries.forEach(e => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
      <td>${e.id}</td><td>${e.artikel}</td><td>${e.anbieter}</td>
      <td>${formatEuroFromCent(e.preisCent)}</td><td>${formatMenge(e.mengeWert, e.mengeEinheit)}</td>
      <td class="action-buttons">
        <button type="button">Bearbeiten</button>
        <button type="button" class="danger">Löschen</button>
      </td>`;
        const [editBtn, delBtn] = tr.querySelectorAll('button');
        editBtn.onclick = () => openEntryEditor(e);
        delBtn.onclick = () => deleteEntry(e.id);
        tbody.append(tr);
    });
    updateCompareSelectors();
}

function openEntryEditor(e) {
    const f = el('edit-entry-form');
    f.hidden = false;
    el('edit-id').value = e.id;
    el('edit-artikel').value = e.artikel;
    el('edit-anbieter').value = e.anbieter;
    el('edit-preis').value = (e.preisCent / 100).toFixed(2);
    el('edit-mengewert').value = e.mengeWert ?? '';
    el('edit-mengeeinheit').value = e.mengeEinheit || 'g';
    f.scrollIntoView({ behavior: 'smooth', block: 'center' });
}
const closeEntryEditor = () => { const f = el('edit-entry-form'); f.hidden = true; f.reset(); };

async function loadDatabases() {
    try {
        const d = await apiFetch('/api/db-files');
        state.databases = d.files.map(f => f.name);
        state.currentDb ||= state.databases[0];
        fillDatabaseSelect();
        if (state.currentDb) await loadDatabase(state.currentDb);
    } catch (e) { alert(e.message); }
}

async function loadDatabase(n) {
    if (!n) return;
    try {
        const d = await apiFetch(`/api/db?name=${encodeURIComponent(n)}`);
        state.currentDb = d.name; state.entries = d.entries;
        fillDatabaseSelect(); renderDatabaseTable();
    } catch (e) { alert(e.message); }
}

async function modifyEntry(path, form, ids) {
    const preis = toCentFromEuro(el(`${form}-preis`).value);
    if (preis == null) return alert('Bitte gültigen Preis angeben.');
    const mengeWertInput = el(`${form}-mengewert`);
    const mengeEinheitInput = el(`${form}-mengeeinheit`);
    const mengeWert = parseFloat(mengeWertInput.value.replace(',', '.'));
    if (!Number.isFinite(mengeWert) || mengeWert <= 0) return alert('Bitte gültigen Mengenwert angeben.');
    const mengeEinheit = mengeEinheitInput.value;
    if (!mengeEinheit) return alert('Bitte Einheit wählen.');
    const body = createFormBody({
        name: state.currentDb,
        id: el(`${form}-id`).value,
        artikel: el(`${form}-artikel`).value.trim(),
        anbieter: el(`${form}-anbieter`).value.trim(),
        preisCent: preis,
        mengeWert: mengeWert,
        mengeEinheit
    });
    try {
        await apiFetch(path, { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
        if (form==='edit') closeEntryEditor();
        await loadDatabase(state.currentDb);
    } catch (e) { alert(e.message); }
}

const addEntry = e => (e.preventDefault(), modifyEntry('/api/db/add','add'));
const saveEntry = e => (e.preventDefault(), modifyEntry('/api/db/update','edit'));

async function deleteEntry(id) {
    if (!state.currentDb || !confirm('Eintrag löschen?')) return;
    try {
        await apiFetch('/api/db/delete', {
            method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'},
            body:createFormBody({ name:state.currentDb, id })
        });
        closeEntryEditor(); await loadDatabase(state.currentDb);
    } catch (e) { alert(e.message); }
}

async function loadList() {
    try {
        const d = await apiFetch('/api/list');
        state.list = d.items; renderList();
    } catch (e) { alert(e.message); }
}

function renderList() {
    const tbody = document.querySelector('#list-table tbody');
    tbody.innerHTML = '';
    state.list.forEach((x,i) => {
        const tr = document.createElement('tr');
        tr.innerHTML = `<td>${i+1}</td><td>${x.artikel}</td><td>${x.anbieter||'–'}</td>
      <td><button>Bearbeiten</button><button class="danger">Löschen</button></td>`;
        const [editBtn, delBtn] = tr.querySelectorAll('button');
        editBtn.onclick = () => openListEditor(i);
        delBtn.onclick = () => deleteListItem(i);
        tbody.append(tr);
    });
}

function openListEditor(i) {
    const e = state.list[i], f = el('edit-list-form');
    f.hidden = false;
    el('list-edit-index').value = i;
    el('list-edit-artikel').value = e.artikel;
    el('list-edit-anbieter').value = e.anbieter || '';
    f.scrollIntoView({ behavior:'smooth', block:'center' });
}
const closeListEditor = () => { const f=el('edit-list-form'); f.hidden=true; f.reset(); };

async function modifyList(path, form) {
    const body = createFormBody({
        index: el(`list-${form}-index`)?.value,
        artikel: el(`list-${form}-artikel`).value.trim(),
        anbieter: el(`list-${form}-anbieter`).value.trim()
    });
    try {
        await apiFetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
        closeListEditor(); await loadList();
    } catch(e){alert(e.message);}
}
const addListItem = e => (e.preventDefault(), modifyList('/api/list/add','add'));
const saveListItem = e => (e.preventDefault(), modifyList('/api/list/update','edit'));
async function deleteListItem(i){
    if(!confirm('Entfernen?'))return;
    try{
        await apiFetch('/api/list/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:createFormBody({index:i})});
        closeListEditor(); await loadList();
    }catch(e){alert(e.message);}
}

function updateCompareSelectors(){
    const [a,b]=['compare-first','compare-second'].map(el);
    const o=e=>`<option value="${e.id}">${e.id} • ${e.artikel} (${e.anbieter})</option>`;
    [a,b].forEach(s=>s.innerHTML=state.entries.map(o).join(''));
}

async function compareSingle(e){
    e.preventDefault();
    if(!state.currentDb||!state.entries.length)return alert('Datenbank laden.');
    const body=createFormBody({
        name:state.currentDb,
        firstId:el('compare-first').value,
        secondId:el('compare-second').value,
        amount:el('compare-amount').value
    });
    const out=el('single-compare-result'); out.textContent='Berechne...';
    try{
        const d=await apiFetch('/api/compare/single',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
        const p=v=>(v/100).toFixed(2), u=v=>(v/100).toFixed(4);
        const res=`Vergleich für ${d.amount} ${d.unit}
${d.first.artikel} (${d.first.anbieter}): ${p(d.first.total)} €, ${u(d.first.unitPrice)} €/ ${d.unit}
${d.second.artikel} (${d.second.anbieter}): ${p(d.second.total)} €, ${u(d.second.unitPrice)} €/ ${d.unit}
→ ${d.cheaper==='first'?`${d.first.artikel} ist günstiger.`:d.cheaper==='second'?`${d.second.artikel} ist günstiger.`:'Gleich teuer.'}`;
        out.textContent=res;
    }catch(x){out.textContent=x.message;}
}

async function compareList(e){
    e.preventDefault();
    if(!state.currentDb)return alert('Datenbank laden.');
    const body=createFormBody({name:state.currentDb,apply:el('apply-best-offers').checked?'1':'0'});
    const c=el('list-compare-result'); c.textContent='Analysiere...';
    try{
        const d=await apiFetch('/api/compare/list',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
        c.innerHTML=d.items.length?d.items.map(i=>{
            if(!i.anbieterGefunden)return `<div>${i.artikel}: Kein Anbieter gefunden.</div>`;
            if(i.empfehlung){
                const mengeText=formatMenge(i.empfehlung.mengeWert, i.empfehlung.mengeEinheit);
                const p=(i.empfehlung.preisCent/100).toFixed(2);
                const u=i.empfehlung.unit?`, ${(i.empfehlung.unitPrice/100).toFixed(4)} €/ ${i.empfehlung.unit}`:'';
                const mengeHinweis=mengeText?` für ${mengeText}`:'';
                return `<div>${i.artikel}: Bester Anbieter ${i.empfehlung.anbieter} (${p} €${mengeHinweis}${u}).</div>`;
            }
            return `<div>${i.artikel}: Keine Empfehlung.</div>`;
        }).join(''):'<p>Liste leer.</p>';
        if(d.updated)await loadList();
    }catch(x){c.textContent=x.message;}
}

function initEventListeners(){
    el('database-select').onchange=e=>loadDatabase(e.target.value);
    el('reload-database').onclick=()=>state.currentDb?loadDatabase(state.currentDb):loadDatabases();
    el('add-entry-form').onsubmit=addEntry;
    el('edit-entry-form').onsubmit=saveEntry;
    el('cancel-edit').onclick=closeEntryEditor;
    el('add-list-form').onsubmit=addListItem;
    el('edit-list-form').onsubmit=saveListItem;
    el('cancel-list-edit').onclick=closeListEditor;
    el('single-compare-form').onsubmit=compareSingle;
    el('list-compare-form').onsubmit=compareList;
}
(async()=>{initEventListeners();await loadDatabases();await loadList();})();
