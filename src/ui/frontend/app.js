document.addEventListener('DOMContentLoaded', () => {
    // Basic file tree interaction
    const folders = document.querySelectorAll('.folder');
    folders.forEach(folder => {
        folder.addEventListener('click', (e) => {
            if (e.target === folder || e.target.parentElement === folder && e.target.classList.contains('icon')) {
                folder.classList.toggle('open');
            }
        });
    });

    // Close window via C++ Webview IPC (if running in webview)
    const closeBtn = document.querySelector('.control.close');
    closeBtn.addEventListener('click', () => {
        if (window.chrome && window.chrome.webview) {
            window.chrome.webview.postMessage({ type: 'close' });
        } else {
            console.log("Not running in Webview environment.");
        }
    });

    console.log("Vrutti UI initialized.");
});
