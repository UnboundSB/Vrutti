export interface TaskDefinition {
    label: string;
    command: string;
    isDefaultBuild?: boolean;
}

export interface TasksConfiguration {
    version: string;
    tasks: TaskDefinition[];
}

export class VruttiTaskManager {
    static async getTasksConfig(): Promise<TasksConfiguration | null> {
        if (!(window as any).vruttiReadFile) return null;
        try {
            let actualDir = (window as any).currentWorkspace || '.';
            if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
            else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);

            if (actualDir.endsWith('/') || actualDir.endsWith('\\')) {
                actualDir = actualDir.substring(0, actualDir.length - 1);
            }

            const tasksPath = actualDir + '/.vrutti/tasks.json';
            const content = await (window as any).vruttiReadFile(tasksPath);
            if (!content) return null;

            return JSON.parse(content) as TasksConfiguration;
        } catch (e) {
            console.error("Failed to read tasks.json:", e);
            return null;
        }
    }

    static async saveTasksConfig(config: TasksConfiguration): Promise<boolean> {
        if (!(window as any).vruttiWriteFile) return false;
        try {
            let actualDir = (window as any).currentWorkspace || '.';
            if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
            else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);

            if (actualDir.endsWith('/') || actualDir.endsWith('\\')) {
                actualDir = actualDir.substring(0, actualDir.length - 1);
            }

            const dirPath = actualDir + '/.vrutti';
            const tasksPath = dirPath + '/tasks.json';
            
            await (window as any).vruttiWriteFile(tasksPath, JSON.stringify(config, null, 2));
            return true;
        } catch (e) {
            console.error("Failed to write tasks.json:", e);
            return false;
        }
    }

    static async runTask(task: TaskDefinition) {
        window.dispatchEvent(new CustomEvent('vrutti-ipc', {
            detail: {
                method: 'terminal/runCommand',
                params: { command: task.command }
            }
        }));
    }

    static async runDefaultBuildTask() {
        const config = await this.getTasksConfig();
        if (config && config.tasks) {
            const defaultTask = config.tasks.find(t => t.isDefaultBuild);
            if (defaultTask) {
                this.runTask(defaultTask);
                return true;
            }
        }
        return false;
    }

    static async ensureTasksFileExists(): Promise<string | null> {
        let config = await this.getTasksConfig();
        if (!config) {
            config = {
                version: "1.0.0",
                tasks: [
                    {
                        label: "Echo Task",
                        command: "echo 'Hello from Vrutti Tasks!'"
                    }
                ]
            };
            await this.saveTasksConfig(config);
        }
        
        let actualDir = (window as any).currentWorkspace || '.';
        if (actualDir.startsWith('file:///')) actualDir = actualDir.substring(8);
        else if (actualDir.startsWith('file://')) actualDir = actualDir.substring(7);
        if (actualDir.endsWith('/') || actualDir.endsWith('\\')) {
            actualDir = actualDir.substring(0, actualDir.length - 1);
        }
        return "file:///" + actualDir + '/.vrutti/tasks.json';
    }
}
