(function(){
    const datetimeEl = document.getElementById('datetime');
    if (!datetimeEl) return;

    function updateDateTime() {
        const now = new Date();
        const options = { weekday: 'short', day: '2-digit', month: '2-digit', year: 'numeric' };
        const date = now.toLocaleDateString('de-DE', options);
        const time = now.toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
        datetimeEl.textContent = `${date} ${time}`;
    }

    updateDateTime();
    setInterval(updateDateTime, 1000);
})();
