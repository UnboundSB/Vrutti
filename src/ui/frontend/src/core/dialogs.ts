// The questions and the news of the editor.
//
// The page may not ask the browser to ask the user: the browser of the
// editor does not show those questions, so the answer is always "no" and
// the action never happens. The question is asked in the page itself, and
// the news is shown in the page itself.

/**
 * Asks the user a question and waits for the answer.
 *
 * The question is shown in the middle of the page. The answer is true when
 * the user says yes.
 */
export function confirmInPage(message: string, detail = '', yes = 'Yes'): Promise<boolean> {
    return new Promise<boolean>((resolve) => {
        const backdrop = document.createElement('div');
        backdrop.setAttribute('style', [
            'position: fixed', 'inset: 0', 'z-index: 99999',
            'background: rgba(0,0,0,0.45)',
            'display: flex', 'align-items: center', 'justify-content: center',
            'font-family: var(--vrutti-font, sans-serif)'
        ].join(';'));

        const box = document.createElement('div');
        box.setAttribute('style', [
            'background: var(--vrutti-surface, #1a1b26)',
            'border: 1px solid var(--vrutti-surface-border, #1f2335)',
            'border-radius: 6px', 'padding: 18px 20px',
            'min-width: 320px', 'max-width: 480px',
            'color: var(--vrutti-text-bright, #c0caf5)',
            'box-shadow: 0 12px 40px rgba(0,0,0,0.5)'
        ].join(';'));

        const title = document.createElement('div');
        title.textContent = message;
        title.setAttribute('style', 'font-size: 13px; margin-bottom: 6px;');

        const sub = document.createElement('div');
        sub.textContent = detail;
        sub.setAttribute('style',
            'font-size: 12px; color: var(--vrutti-text, #717cb4); margin-bottom: 16px;');

        const row = document.createElement('div');
        row.setAttribute('style', 'display: flex; gap: 8px; justify-content: flex-end;');

        const cancel = document.createElement('button');
        cancel.textContent = 'Cancel';
        const ok = document.createElement('button');
        ok.textContent = yes;

        const styleButton = (button: HTMLElement, danger: boolean) => button.setAttribute('style', [
            'background: ' + (danger ? '#e81123' : 'transparent'),
            'color: ' + (danger ? '#ffffff' : 'var(--vrutti-text-bright, #c0caf5)'),
            'border: 1px solid ' + (danger ? '#e81123' : 'var(--vrutti-surface-border, #1f2335)'),
            'border-radius: 4px', 'padding: 5px 14px', 'font-size: 12px', 'cursor: pointer'
        ].join(';'));

        styleButton(cancel, false);
        styleButton(ok, true);

        const finish = (answer: boolean) => {
            backdrop.remove();
            resolve(answer);
        };

        cancel.addEventListener('click', () => finish(false));
        ok.addEventListener('click', () => finish(true));
        backdrop.addEventListener('click', (e) => {
            if (e.target === backdrop) finish(false);
        });

        row.appendChild(cancel);
        row.appendChild(ok);
        box.appendChild(title);
        if (detail) box.appendChild(sub);
        box.appendChild(row);
        backdrop.appendChild(box);
        document.body.appendChild(backdrop);
        ok.focus();
    });
}

/**
 * Shows a piece of news to the user.
 *
 * The news goes away by itself after a while.
 */
export function notifyInPage(message: string): void {
    const toast = document.createElement('div');
    toast.textContent = message;
    toast.setAttribute('style', [
        'position: fixed', 'right: 16px', 'bottom: 16px', 'z-index: 99999',
        'background: var(--vrutti-surface, #1a1b26)',
        'border: 1px solid var(--vrutti-surface-border, #1f2335)',
        'border-left: 3px solid var(--vrutti-accent, #7aa2f7)',
        'border-radius: 4px', 'padding: 10px 14px', 'max-width: 420px',
        'color: var(--vrutti-text-bright, #c0caf5)', 'font-size: 12px',
        'font-family: var(--vrutti-font, sans-serif)',
        'box-shadow: 0 8px 24px rgba(0,0,0,0.45)'
    ].join(';'));
    document.body.appendChild(toast);
    setTimeout(() => toast.remove(), 4000);
}
