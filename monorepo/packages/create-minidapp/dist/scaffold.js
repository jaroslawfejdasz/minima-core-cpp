"use strict";
/**
 * MiniDapp scaffolding engine
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.scaffold = scaffold;
const node_fs_1 = require("node:fs");
const node_path_1 = require("node:path");
function scaffold(targetDir, opts) {
    const { name, description = `${name} MiniDapp`, version = '1.0.0', author = '', category = 'Utility', withContract = false, withMaxima = false, } = opts;
    if ((0, node_fs_1.existsSync)(targetDir)) {
        throw new Error(`Directory already exists: ${targetDir}`);
    }
    (0, node_fs_1.mkdirSync)(targetDir, { recursive: true });
    const created = [];
    function write(relPath, content) {
        const abs = (0, node_path_1.join)(targetDir, relPath);
        const dir = abs.substring(0, abs.lastIndexOf('/'));
        if (dir)
            (0, node_fs_1.mkdirSync)(dir, { recursive: true });
        (0, node_fs_1.writeFileSync)(abs, content, 'utf8');
        created.push(relPath);
    }
    // ── dapp.conf ───────────────────────────────────────────────────────────
    write('dapp.conf', JSON.stringify({
        name,
        description,
        version,
        author,
        category,
        icon: 'icon.png',
        browser: 'internal',
        permission: withMaxima ? 'write' : 'read',
    }, null, 2) + '\n');
    // ── icon placeholder ─────────────────────────────────────────────────────
    // Minimal 1x1 white PNG (base64)
    const PNG_1x1 = Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwADhQGAWjR9awAAAABJRU5ErkJggg==', 'base64');
    (0, node_fs_1.writeFileSync)((0, node_path_1.join)(targetDir, 'icon.png'), PNG_1x1);
    created.push('icon.png');
    // ── index.html ───────────────────────────────────────────────────────────
    write('index.html', `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${name}</title>
  <script src="mds.js"></script>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0f1117;
      color: #e0e0e0;
      min-height: 100vh;
      padding: 1.5rem;
    }
    header {
      display: flex;
      align-items: center;
      gap: 1rem;
      margin-bottom: 2rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid #2a2a3e;
    }
    header img { width: 40px; height: 40px; border-radius: 8px; }
    header h1 { font-size: 1.4rem; font-weight: 700; color: #fff; }
    .card {
      background: #1a1b26;
      border: 1px solid #2a2a3e;
      border-radius: 12px;
      padding: 1.25rem 1.5rem;
      margin-bottom: 1rem;
    }
    .card h2 { font-size: 0.9rem; text-transform: uppercase; letter-spacing: 0.05em; color: #7a7a9a; margin-bottom: 0.75rem; }
    .balance { font-size: 2rem; font-weight: 700; color: #a78bfa; }
    .address { font-family: monospace; font-size: 0.8rem; color: #7a7a9a; word-break: break-all; }
    button {
      background: #7c3aed;
      color: white;
      border: none;
      border-radius: 8px;
      padding: 0.6rem 1.25rem;
      cursor: pointer;
      font-size: 0.95rem;
      margin-top: 0.5rem;
    }
    button:hover { background: #6d28d9; }
    #status { margin-top: 1rem; font-size: 0.85rem; color: #7a7a9a; }
    .loading { opacity: 0.6; font-style: italic; }
  </style>
</head>
<body>
  <header>
    <img src="icon.png" alt="${name}">
    <h1>${name}</h1>
  </header>

  <div class="card">
    <h2>Balance</h2>
    <div class="balance loading" id="balance">Loading…</div>
  </div>

  <div class="card">
    <h2>Address</h2>
    <div class="address loading" id="address">Loading…</div>
    <button onclick="copyAddress()">Copy Address</button>
  </div>

  <div id="status"></div>

  <script src="app.js"></script>
</body>
</html>
`);
    // ── app.js ────────────────────────────────────────────────────────────────
    write('app.js', `// ${name} — MiniDapp application logic
'use strict';

// Initialize MDS when DOM is ready
MDS.init(function(msg) {
  if (msg.event === 'inited') {
    onMDSReady();
  }
});

function onMDSReady() {
  console.log('${name}: MDS initialized');
  loadBalance();
  loadAddress();
}

function loadBalance() {
  MDS.cmd('balance', function(resp) {
    if (resp.status) {
      const miniBalance = resp.response.balance.find(b => b.token === 'Minima');
      document.getElementById('balance').textContent =
        miniBalance ? miniBalance.sendable + ' MINIMA' : '0 MINIMA';
      document.getElementById('balance').classList.remove('loading');
    }
  });
}

function loadAddress() {
  MDS.cmd('getaddress', function(resp) {
    if (resp.status) {
      const addr = resp.response.miniaddress;
      document.getElementById('address').textContent = addr;
      document.getElementById('address').classList.remove('loading');
      document.getElementById('address').dataset.addr = addr;
    }
  });
}

function copyAddress() {
  const addr = document.getElementById('address').dataset.addr;
  if (!addr) return;
  navigator.clipboard.writeText(addr).then(() => {
    setStatus('Address copied!');
  });
}

function setStatus(msg) {
  const el = document.getElementById('status');
  el.textContent = msg;
  setTimeout(() => { el.textContent = ''; }, 3000);
}
`);
    // ── mds.js stub (dev) ─────────────────────────────────────────────────────
    write('mds.js', `/**
 * mds.js — Minima MDS (Minima Dapp System) stub for development
 * 
 * Replace this with the real mds.js from your Minima node for production.
 * Real location: http://localhost:9003/mds.js
 * 
 * This stub allows offline development / testing.
 */
const MDS = {
  init: function(callback) {
    console.log('[MDS stub] Initialized in dev mode');
    setTimeout(() => callback({ event: 'inited' }), 100);
  },
  cmd: function(command, callback) {
    console.log('[MDS stub] CMD:', command);
    const stubs = {
      balance: {
        status: true,
        response: {
          balance: [{ token: 'Minima', sendable: '100.00', total: '100.00' }]
        }
      },
      getaddress: {
        status: true,
        response: { miniaddress: 'MxDEV0000000000000000000000000000000000000000' }
      },
    };
    setTimeout(() => callback(stubs[command] ?? { status: false, error: 'Not implemented in stub' }), 50);
  },
  notify: function(title, message) {
    console.log('[MDS stub] Notify:', title, message);
  },
  log: function(message) {
    console.log('[MDS stub] Log:', message);
  },
};
`);
    // ── service.js (optional background worker) ───────────────────────────────
    if (withMaxima) {
        write('service.js', `/**
 * service.js — Background service worker for ${name}
 * 
 * Runs continuously in the background.
 * Can receive Maxima messages and trigger UI updates.
 */
MDS.init(function(msg) {
  if (msg.event === 'inited') {
    MDS.log('${name} service started');
    startHeartbeat();
  }

  if (msg.event === 'MDS_MAXIMA_MESSAGE') {
    onMaximaMessage(msg.data);
  }
});

function startHeartbeat() {
  setInterval(() => {
    MDS.cmd('block', function(resp) {
      if (resp.status) {
        MDS.log('Current block: ' + resp.response.block);
      }
    });
  }, 30000); // every 30 seconds
}

function onMaximaMessage(data) {
  MDS.log('Maxima message received: ' + JSON.stringify(data));
  // Handle incoming Maxima messages here
}
`);
    }
    // ── KISS VM contract ──────────────────────────────────────────────────────
    if (withContract) {
        write('contracts/main.kiss', `/* ${name} — Main Contract
   
   This is the primary KISS VM script for your MiniDapp.
   It controls how UTxOs can be spent.
   
   Replace with your actual contract logic.
*/

/* Only the owner can spend */
LET ownerAddress = 0xYOUR_ADDRESS_HERE

RETURN SIGNEDBY ownerAddress
`);
    }
    // ── .gitignore ────────────────────────────────────────────────────────────
    write('.gitignore', `node_modules/
*.zip
*.log
`);
    // ── README.md ─────────────────────────────────────────────────────────────
    write('README.md', `# ${name}

${description}

## Structure

\`\`\`
${name}/
├── dapp.conf       # MiniDapp metadata
├── icon.png        # App icon (replace with your own)
├── index.html      # Main UI
├── app.js          # Application logic
├── mds.js          # MDS API (stub for dev, replace with real for prod)
${withMaxima ? '├── service.js      # Background service (Maxima support)\n' : ''}${withContract ? '└── contracts/\n    └── main.kiss   # KISS VM contract\n' : ''}
\`\`\`

## Development

1. Open \`index.html\` in a browser for offline development (uses MDS stub)
2. To test on a real node: copy the folder to your Minima node and install as a MiniDapp

## Deployment

\`\`\`bash
# Package as ZIP
zip -r ${name.toLowerCase().replace(/\s+/g, '-')}.zip . -x "*.git*" -x "node_modules/*"

# Install via Minima node
# Go to: http://localhost:9003 → Dapp Manager → Install → Upload ZIP
\`\`\`

## Resources

- [Minima Docs](https://docs.minima.global)
- [KISS VM Reference](https://docs.minima.global/learn/smart-contracts/kiss-vm)
- [MDS API Reference](https://docs.minima.global/buildondeminima/minidapps/mds)
`);
    return created;
}
//# sourceMappingURL=scaffold.js.map