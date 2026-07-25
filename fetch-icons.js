const https = require('https');
const icons = ['files', 'search', 'source-control', 'debug-alt', 'extensions', 'chevron-left', 'chevron-right', 'close'];

icons.forEach(icon => {
  https.get(`https://raw.githubusercontent.com/microsoft/vscode-codicons/main/src/icons/${icon}.svg`, res => {
    let data = '';
    res.on('data', chunk => data += chunk);
    res.on('end', () => {
      const match = data.match(/<path[^>]*d=\"([^\"]+)\"/);
      if (match) {
        console.log(icon + ':', match[1]);
      }
    });
  });
});
