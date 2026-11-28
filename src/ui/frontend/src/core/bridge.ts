// bridge.ts
//
// The small helper that reads the answer of a binding of the window.
//
// A binding of the window answers with a text. The text is usually a JSON
// text, and the window puts that text into the page as it is, so the page
// usually gets an object. A binding that answers with a text that is not
// JSON gives a plain text.
//
// The helper takes both, so that a caller never has to know which kind of
// answer it is holding. Before, a caller that read `answer.path` on a text
// got nothing at all, and the feature it wanted quietly stopped working:
// the folder of the workspace never came, and the app opened the folder of
// the last time instead.

export function readAnswer(answer: unknown): any {
    if (typeof answer !== 'string') return answer;
    const text = answer.trim();
    if (text.length === 0) return null;
    if (text[0] !== '{' && text[0] !== '[' && text[0] !== '"') return answer;
    try {
        return JSON.parse(text);
    } catch {
        return answer;
    }
}

// The path of a folder that the core has given to the page.
//
// The answer is empty when there is no folder: the app then opens without a
// folder, and the explorer says that no folder is opened.
export function readFolderPath(answer: unknown): string {
    const value = readAnswer(answer);
    if (!value || typeof value !== 'object') return '';
    const path = (value as any).path;
    if (typeof path !== 'string') return '';
    return path;
}
