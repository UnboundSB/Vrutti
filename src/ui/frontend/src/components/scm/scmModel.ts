export interface SCMFile {
  resource: string;
  name: string;
  directory: string;
  status: 'modified' | 'added' | 'deleted' | 'untracked' | 'renamed';
  staged: boolean;
}

export class SCMModel {
  public stagedFiles: SCMFile[] = [];
  public unstagedFiles: SCMFile[] = [];
  public untrackedFiles: SCMFile[] = [];

  parseGitStatus(statusOutput: string) {
    this.stagedFiles = [];
    this.unstagedFiles = [];
    this.untrackedFiles = [];

    const lines = statusOutput.split('\n');
    for (const line of lines) {
      if (line.length < 4) continue;

      const xy = line.substring(0, 2);
      const path = line.substring(3).trim();
      const parts = path.split('/');
      const name = parts.pop() || path;
      const directory = parts.join('/');

      // X is index (staged), Y is working tree (unstaged)
      const x = xy[0];
      const y = xy[1];

      // Handle staged
      if (x !== ' ' && x !== '?') {
        this.stagedFiles.push({
          resource: path,
          name: name,
          directory: directory,
          status: this.mapStatus(x),
          staged: true
        });
      }

      // Handle unstaged / untracked
      if (y !== ' ') {
        if (y === '?') {
          this.untrackedFiles.push({
            resource: path,
            name: name,
            directory: directory,
            status: 'untracked',
            staged: false
          });
        } else {
          this.unstagedFiles.push({
            resource: path,
            name: name,
            directory: directory,
            status: this.mapStatus(y),
            staged: false
          });
        }
      }
    }
  }

  private mapStatus(code: string): SCMFile['status'] {
    switch (code) {
      case 'M': return 'modified';
      case 'A': return 'added';
      case 'D': return 'deleted';
      case 'R': return 'renamed';
      case 'U': return 'modified'; // Updated but unmerged
      default: return 'modified';
    }
  }
}
