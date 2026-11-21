// Auditoría independiente de Claude: Chromium real con viewport de iPhone contra V5 y data/.
// Instalar una vez: cd tests/navegador && npm ci && npx playwright install chromium
import { defineConfig, devices } from '@playwright/test';
export default defineConfig({
  testDir: '.',
  testMatch: '*.spec.mjs',
  outputDir: '../../test-results/navegador',
  timeout: 60000,
  workers: 1,
  reporter: [['list'], ['json', { outputFile: '../../test-results/navegador/resultados.json' }]],
  use: { ...devices['iPhone 13'], browserName: 'chromium', baseURL: 'http://127.0.0.1:8765', serviceWorkers: 'block' },
  webServer: { command: 'node servidor.mjs', url: 'http://127.0.0.1:8765/salud', reuseExistingServer: false },
});
