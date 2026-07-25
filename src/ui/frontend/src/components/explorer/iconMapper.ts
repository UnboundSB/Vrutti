export function getIconForFile(filename: string, isDirectory: boolean, isExpanded: boolean): string {
  const lower = filename.toLowerCase();

  if (isDirectory) {
    if (lower === 'node_modules') return isExpanded ? './icons/folder-node-open.svg' : './icons/folder-node.svg';
    if (lower === 'src') return isExpanded ? './icons/folder-src-open.svg' : './icons/folder-src.svg';
    if (lower === 'public') return isExpanded ? './icons/folder-public-open.svg' : './icons/folder-public.svg';
    if (lower === 'components') return isExpanded ? './icons/folder-components-open.svg' : './icons/folder-components.svg';
    if (lower === '.git') return isExpanded ? './icons/folder-git-open.svg' : './icons/folder-git.svg';
    return isExpanded ? './icons/folder-open.svg' : './icons/folder.svg';
  }

  if (lower === 'package.json') return './icons/nodejs.svg';
  if (lower.includes('tsconfig')) return './icons/tsconfig.svg';
  if (lower === 'vite.config.ts') return './icons/vite.svg';
  if (lower === '.gitignore') return './icons/git.svg';
  if (lower === '.env' || lower.startsWith('.env.')) return './icons/tune.svg';
  if (lower.endsWith('.ts')) return './icons/typescript.svg';
  if (lower.endsWith('.js')) return './icons/javascript.svg';
  if (lower.endsWith('.json')) return './icons/json.svg';
  if (lower.endsWith('.md')) return './icons/markdown.svg';
  if (lower.endsWith('.html')) return './icons/html.svg';
  if (lower.endsWith('.css')) return './icons/css.svg';
  if (lower.endsWith('.cpp') || lower.endsWith('.cxx') || lower.endsWith('.cc')) return './icons/cpp.svg';
  if (lower.endsWith('.c')) return './icons/c.svg';
  if (lower.endsWith('.cs')) return './icons/csharp.svg';
  if (lower.endsWith('.h') || lower.endsWith('.hpp')) return './icons/h.svg';
  if (lower.endsWith('.svg')) return './icons/svg.svg';
  if (lower.endsWith('.png') || lower.endsWith('.jpg') || lower.endsWith('.jpeg') || lower.endsWith('.gif')) return './icons/image.svg';
  if (lower.endsWith('.yml') || lower.endsWith('.yaml')) return './icons/yaml.svg';
  if (lower.endsWith('.bin') || lower.endsWith('.exe') || lower.endsWith('.sh') || lower.endsWith('.bat') || lower.endsWith('.cmd')) return './icons/console.svg';
  if (lower.endsWith('.py') || lower.endsWith('.pyc')) return './icons/python.svg';
  if (lower.endsWith('.txt')) return './icons/document.svg';
  if (lower.endsWith('.java')) return './icons/java.svg';
  if (lower.endsWith('.go')) return './icons/go.svg';
  if (lower.endsWith('.rs')) return './icons/rust.svg';
  if (lower.endsWith('.xml')) return './icons/xml.svg';
  if (lower.endsWith('.sql')) return './icons/database.svg';
  if (lower.endsWith('.lock')) return './icons/lock.svg';
  if (lower.endsWith('.rb')) return './icons/ruby.svg';
  if (lower.endsWith('.php')) return './icons/php.svg';
  if (lower.endsWith('.swift')) return './icons/swift.svg';
  
  return './icons/file.svg';
}
