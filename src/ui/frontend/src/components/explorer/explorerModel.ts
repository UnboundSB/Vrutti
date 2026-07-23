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
  
  constructor(stat: IFileStat) {
    this.name = stat.name;
    this.isDirectory = stat.isDirectory;
    this.resource = stat.resource;
    if (stat.children) {
      this.children = stat.children.map(c => new ExplorerItem(c));
    }
  }

  public toggle(): void {
    if (this.isDirectory) {
      this.isExpanded = !this.isExpanded;
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
