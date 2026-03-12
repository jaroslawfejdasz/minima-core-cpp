#!/usr/bin/env node
// Autonomous work skill — runs as part of scheduled automation
// Reads state, does next chunk of work, updates state, reports

import { createClient } from '@base44/sdk';
import fs from 'fs';

const base44 = createClient({
  appId: process.env.BASE44_APP_ID,
  token: process.env.BASE44_SERVICE_TOKEN,
  serverUrl: process.env.BASE44_API_URL,
});

const STATE_FILE = '/app/AGENT_STATE.json';

async function main() {
  const state = JSON.parse(fs.readFileSync(STATE_FILE, 'utf8'));
  console.log('Current state:', JSON.stringify(state, null, 2));
  // The agent itself handles the logic — this just confirms the skill is ready
  console.log('Autonomous work skill loaded OK');
}

main().catch(console.error);
