const state = {
    databases: [],
    currentDb: null,
    entries: [],
    list: []
};

function formatEuroFromCent(value) {
    return (value / 100).toFixed(2).replace('.', ',');
}

function toCentFromEuro(value) {
    const number = parseFloat(String(value).replace(',', '.'));
    if (Number.isNaN(number)) return null;
    return Math.round(number * 100);
}

async function apiFetch(path, options = {}) {
    const response = await fetch(path, options);
    const contentType = response.headers.get('content-type') || '';
    if (!response.ok) {
        if (contentType.includes('application/json')) {
            const payload = await response.json().catch(() => ({}));
            throw new Error(payload.error || `Fehler ${response.status}`);
        }
        throw new Error(`Fehler ${response.status}`);
    }
    if (contentType.includes('application/json')) {
        return response.json();
    }
    return response.text();
}

function fillDatabaseSelect() {
    const select = document.getElementById('database-select');
    select.innerHTML = '';
    state.databases.forEach((name) => {
        const option = document.createElement('option');
        option.value = name;
        option.textContent = name;
        select.append(option);
    });
    if (state.currentDb) {
        select.value = state.currentDb;
    }
}

function renderDatabaseTable() {
    const tbody = document.querySelector('#database-table tbody');
    tbody.innerHTML = '';
    state.entries.forEach((entry) => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>${entry.id}</td>
            <td>${entry.artikel}</td>
            <td>${entry.anbieter}</td>
            <td>${formatEuroFromCent(entry.preisCent)}</td>
            <td>${entry.menge}</td>
            <td class="action-buttons"></td>`;
        const actions = tr.querySelector('.action-buttons');
        const editBtn = document.createElement('button');
        editBtn.textContent = 'Bearbeiten';
        editBtn.type = 'button';
        editBtn.addEventListener('click', () => openEntryEditor(entry));
        const deleteBtn = document.createElement('button');
        deleteBtn.textContent = 'Löschen';
        deleteBtn.type = 'button';
        deleteBtn.classList.add('danger');
        deleteBtn.addEventListener('click', () => deleteEntry(entry.id));
        actions.append(editBtn, deleteBtn);
        tbody.append(tr);
    });
    updateCompareSelectors();
}

function openEntryEditor(entry) {
    const form = document.getElementById('edit-entry-form');
    form.hidden = false;
    document.getElementById('edit-id').value = entry.id;
    document.getElementById('edit-artikel').value = entry.artikel;
    document.getElementById('edit-anbieter').value = entry.anbieter;
    document.getElementById('edit-preis').value = (entry.preisCent / 100).toFixed(2);
    document.getElementById('edit-menge').value = entry.menge;
    form.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

function closeEntryEditor() {
    const form = document.getElementById('edit-entry-form');
    form.hidden = true;
    form.reset();
}

async function loadDatabases() {
    try {
        const data = await apiFetch('/api/db-files');
        state.databases = data.files.map((file) => file.name);
        if (!state.currentDb && state.databases.length > 0) {
            state.currentDb = state.databases[0];
        }
        fillDatabaseSelect();
        if (state.currentDb) {
            await loadDatabase(state.currentDb);
        }
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

async function loadDatabase(name) {
    if (!name) return;
    try {
        const data = await apiFetch(`/api/db?name=${encodeURIComponent(name)}`);
        state.currentDb = data.name;
        state.entries = data.entries;
        fillDatabaseSelect();
        renderDatabaseTable();
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

function createFormBody(params) {
    const body = new URLSearchParams();
    Object.entries(params).forEach(([key, value]) => {
        if (value !== undefined && value !== null) {
            body.append(key, value);
        }
    });
    return body;
}

async function addEntry(event) {
    event.preventDefault();
    if (!state.currentDb) return;
    const preis = toCentFromEuro(document.getElementById('add-preis').value);
    if (preis === null) {
        alert('Bitte einen gültigen Preis angeben.');
        return;
    }
    const body = createFormBody({
        name: state.currentDb,
        id: document.getElementById('add-id').value,
        artikel: document.getElementById('add-artikel').value.trim(),
        anbieter: document.getElementById('add-anbieter').value.trim(),
        preisCent: preis,
        menge: document.getElementById('add-menge').value.trim()
    });
    try {
        await apiFetch('/api/db/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        event.target.reset();
        await loadDatabase(state.currentDb);
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

async function saveEntry(event) {
    event.preventDefault();
    if (!state.currentDb) return;
    const preis = toCentFromEuro(document.getElementById('edit-preis').value);
    if (preis === null) {
        alert('Bitte einen gültigen Preis angeben.');
        return;
    }
    const body = createFormBody({
        name: state.currentDb,
        id: document.getElementById('edit-id').value,
        artikel: document.getElementById('edit-artikel').value.trim(),
        anbieter: document.getElementById('edit-anbieter').value.trim(),
        preisCent: preis,
        menge: document.getElementById('edit-menge').value.trim()
    });
    try {
        await apiFetch('/api/db/update', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        closeEntryEditor();
        await loadDatabase(state.currentDb);
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

async function deleteEntry(id) {
    if (!state.currentDb) return;
    if (!confirm('Diesen Eintrag wirklich löschen?')) return;
    const body = createFormBody({ name: state.currentDb, id });
    try {
        await apiFetch('/api/db/delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        if (!document.getElementById('edit-entry-form').hidden && document.getElementById('edit-id').value === String(id)) {
            closeEntryEditor();
        }
        await loadDatabase(state.currentDb);
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

function renderList() {
    const tbody = document.querySelector('#list-table tbody');
    tbody.innerHTML = '';
    state.list.forEach((item, index) => {
        const tr = document.createElement('tr');
        tr.innerHTML = `
            <td>${index + 1}</td>
            <td>${item.artikel}</td>
            <td>${item.anbieter || '–'}</td>
            <td class="action-buttons"></td>`;
        const actions = tr.querySelector('.action-buttons');
        const editBtn = document.createElement('button');
        editBtn.textContent = 'Bearbeiten';
        editBtn.type = 'button';
        editBtn.addEventListener('click', () => openListEditor(index));
        const deleteBtn = document.createElement('button');
        deleteBtn.textContent = 'Löschen';
        deleteBtn.type = 'button';
        deleteBtn.classList.add('danger');
        deleteBtn.addEventListener('click', () => deleteListItem(index));
        actions.append(editBtn, deleteBtn);
        tbody.append(tr);
    });
}

function openListEditor(index) {
    const entry = state.list[index];
    const form = document.getElementById('edit-list-form');
    form.hidden = false;
    document.getElementById('list-edit-index').value = index;
    document.getElementById('list-edit-artikel').value = entry.artikel;
    document.getElementById('list-edit-anbieter').value = entry.anbieter || '';
    form.scrollIntoView({ behavior: 'smooth', block: 'center' });
}

function closeListEditor() {
    const form = document.getElementById('edit-list-form');
    form.hidden = true;
    form.reset();
}

async function loadList() {
    try {
        const data = await apiFetch('/api/list');
        state.list = data.items.map((item) => ({
            index: item.index,
            artikel: item.artikel,
            anbieter: item.anbieter
        }));
        renderList();
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

async function addListItem(event) {
    event.preventDefault();
    const artikel = document.getElementById('list-add-artikel').value.trim();
    const anbieter = document.getElementById('list-add-anbieter').value.trim();
    const body = createFormBody({ artikel, anbieter });
    try {
        await apiFetch('/api/list/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        event.target.reset();
        await loadList();
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

async function saveListItem(event) {
    event.preventDefault();
    const index = document.getElementById('list-edit-index').value;
    const artikel = document.getElementById('list-edit-artikel').value.trim();
    const anbieter = document.getElementById('list-edit-anbieter').value.trim();
    const body = createFormBody({ index, artikel, anbieter });
    try {
        await apiFetch('/api/list/update', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        closeListEditor();
        await loadList();
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

async function deleteListItem(index) {
    if (!confirm('Eintrag aus der Liste entfernen?')) return;
    const body = createFormBody({ index });
    try {
        await apiFetch('/api/list/delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        if (!document.getElementById('edit-list-form').hidden && document.getElementById('list-edit-index').value === String(index)) {
            closeListEditor();
        }
        await loadList();
    } catch (error) {
        console.error(error);
        alert(error.message);
    }
}

function updateCompareSelectors() {
    const selectFirst = document.getElementById('compare-first');
    const selectSecond = document.getElementById('compare-second');
    const createOption = (entry) => {
        const option = document.createElement('option');
        option.value = entry.id;
        option.textContent = `${entry.id} • ${entry.artikel} (${entry.anbieter})`;
        return option;
    };
    [selectFirst, selectSecond].forEach((select) => {
        select.innerHTML = '';
        state.entries.forEach((entry) => select.append(createOption(entry)));
    });
}

async function compareSingle(event) {
    event.preventDefault();
    if (!state.currentDb || state.entries.length === 0) {
        alert('Bitte zuerst eine Datenbank laden.');
        return;
    }
    const amount = document.getElementById('compare-amount').value;
    const body = createFormBody({
        name: state.currentDb,
        firstId: document.getElementById('compare-first').value,
        secondId: document.getElementById('compare-second').value,
        amount
    });
    const output = document.getElementById('single-compare-result');
    output.textContent = 'Berechne...';
    try {
        const data = await apiFetch('/api/compare/single', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        const firstTotalEuro = (data.first.total / 100).toFixed(2);
        const secondTotalEuro = (data.second.total / 100).toFixed(2);
        const firstUnit = (data.first.unitPrice / 100).toFixed(4);
        const secondUnit = (data.second.unitPrice / 100).toFixed(4);
        let verdict = 'Beide Produkte sind gleich teuer.';
        if (data.cheaper === 'first') {
            verdict = `${data.first.artikel} bei ${data.first.anbieter} ist günstiger.`;
        } else if (data.cheaper === 'second') {
            verdict = `${data.second.artikel} bei ${data.second.anbieter} ist günstiger.`;
        }
        output.textContent = `Vergleich für ${data.amount} ${data.unit}\n` +
            `${data.first.artikel} (${data.first.anbieter}): ${firstTotalEuro} € gesamt, ${firstUnit} €/ ${data.unit}\n` +
            `${data.second.artikel} (${data.second.anbieter}): ${secondTotalEuro} € gesamt, ${secondUnit} €/ ${data.unit}\n` +
            `→ ${verdict}`;
    } catch (error) {
        console.error(error);
        output.textContent = error.message;
    }
}

async function compareList(event) {
    event.preventDefault();
    if (!state.currentDb) {
        alert('Bitte zuerst eine Datenbank laden.');
        return;
    }
    const apply = document.getElementById('apply-best-offers').checked ? '1' : '0';
    const body = createFormBody({ name: state.currentDb, apply });
    const container = document.getElementById('list-compare-result');
    container.textContent = 'Analysiere...';
    try {
        const data = await apiFetch('/api/compare/list', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body
        });
        const fragment = document.createDocumentFragment();
        if (data.items.length === 0) {
            const p = document.createElement('p');
            p.textContent = 'Die Einkaufsliste ist leer.';
            fragment.append(p);
        } else {
            data.items.forEach((item) => {
                const wrapper = document.createElement('div');
                wrapper.className = 'result-item';
                if (item.anbieterGefunden === 0) {
                    wrapper.textContent = `${item.artikel}: Kein Anbieter in der Datenbank gefunden.`;
                } else if (item.empfehlung) {
                    const packPrice = (item.empfehlung.preisCent / 100).toFixed(2);
                    const unitPart = item.empfehlung.unit ? `, ${(item.empfehlung.unitPrice / 100).toFixed(4)} €/ ${item.empfehlung.unit}` : '';
                    wrapper.textContent = `${item.artikel}: Bester Anbieter ${item.empfehlung.anbieter} (${packPrice} € pro Packung${unitPart}).`;
                } else {
                    wrapper.textContent = `${item.artikel}: Keine Empfehlung verfügbar.`;
                }
                fragment.append(wrapper);
            });
        }
        container.innerHTML = '';
        container.append(fragment);
        if (data.updated) {
            await loadList();
        }
    } catch (error) {
        console.error(error);
        container.textContent = error.message;
    }
}

function initEventListeners() {
    document.getElementById('database-select').addEventListener('change', (event) => {
        loadDatabase(event.target.value);
    });
    document.getElementById('reload-database').addEventListener('click', () => {
        if (state.currentDb) {
            loadDatabase(state.currentDb);
        } else {
            loadDatabases();
        }
    });
    document.getElementById('add-entry-form').addEventListener('submit', addEntry);
    document.getElementById('edit-entry-form').addEventListener('submit', saveEntry);
    document.getElementById('cancel-edit').addEventListener('click', closeEntryEditor);
    document.getElementById('add-list-form').addEventListener('submit', addListItem);
    document.getElementById('edit-list-form').addEventListener('submit', saveListItem);
    document.getElementById('cancel-list-edit').addEventListener('click', closeListEditor);
    document.getElementById('single-compare-form').addEventListener('submit', compareSingle);
    document.getElementById('list-compare-form').addEventListener('submit', compareList);
}

(async function init() {
    initEventListeners();
    await loadDatabases();
    await loadList();
})();

