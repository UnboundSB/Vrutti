export interface IFileStat {
  name: string;
  isDirectory: boolean;
  resource: string;
  children?: IFileStat[];
}

export class ExplorerItem {
  public name: string;
  public isDirectory: boolean;
  public resource: string;
  public children: ExplorerItem[] = [];
  public isExpanded: boolean = false;
  public childrenLoaded: boolean = false;
  
  constructor(stat: IFileStat) {
    this.name = stat.name;
    this.isDirectory = stat.isDirectory;
    this.resource = stat.resource;
    if (stat.children) {
      this.children = stat.children.map(c => new ExplorerItem(c));
      this.childrenLoaded = true;
    }
  }

  public async toggle(): Promise<void> {
    if (this.isDirectory) {
      this.isExpanded = !this.isExpanded;
      if (this.isExpanded && !this.childrenLoaded) {
        await this.loadChildren();
      }
    }
  }

  public async loadChildren(): Promise<void> {
    if (!this.isDirectory) return;
    try {
      let path = this.resource;
      if (path.startsWith('file:///')) {
        path = path.substring(8);
      } else if (path.startsWith('file://')) {
        path = path.substring(7);
      }
      const raw = await (window as any).vruttiReadDirectory(path);
      const stats: IFileStat[] = JSON.parse(raw);
      // Sort: directories first, then alphabetical
      stats.sort((a, b) => {
        if (a.isDirectory && !b.isDirectory) return -1;
        if (!a.isDirectory && b.isDirectory) return 1;
        return a.name.localeCompare(b.name);
      });
      this.children = stats.map(s => new ExplorerItem(s));
      this.childrenLoaded = true;
    } catch (e) {
      console.error("Failed to read directory", e);
    }
  }
}

export class ExplorerModel {
  public root: ExplorerItem | null = null;
  
  public setRoot(stat: IFileStat): void {
    this.root = new ExplorerItem(stat);
    this.root.isExpanded = true;
  }
}
