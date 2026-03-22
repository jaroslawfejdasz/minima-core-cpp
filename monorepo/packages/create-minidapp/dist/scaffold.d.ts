/**
 * MiniDapp scaffolding engine
 */
export interface ScaffoldOptions {
    name: string;
    description?: string;
    version?: string;
    author?: string;
    category?: 'Business' | 'Utility' | 'DeFi' | 'NFT' | 'Social';
    withContract?: boolean;
    withMaxima?: boolean;
}
export declare function scaffold(targetDir: string, opts: ScaffoldOptions): string[];
//# sourceMappingURL=scaffold.d.ts.map