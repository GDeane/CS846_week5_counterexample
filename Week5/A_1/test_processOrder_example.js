// Quick harness to exercise the legacy-style processOrder function
// Run with: node test_processOrder_example.js

const path = require('path');
const { processOrder, globalState, cache } = require('./processOrder_example');

function runProcess(orderId) {
  return new Promise((resolve, reject) => {
    processOrder(
      orderId,
      {
        amount: 42,
        template: 'receipt',
        meta: { startedBy: 'test-harness' },
        flag: 'expedite',
        configPath: path.join(__dirname, 'nonexistent_config.json'), // forces fallback config
      },
      (err, result) => {
        if (err) return reject(err);
        console.log('Callback result:', result);
        // Wait for delayed completion side-effect to fire
        setTimeout(() => {
          console.log('Global state:', globalState);
          console.log('Order cache entry:', cache.order.get(orderId));
          resolve();
        }, 400);
      }
    );
  });
}

(async () => {
  try {
    const orderId = `order-${Date.now()}`;
    await runProcess(orderId);
    console.log('Test harness finished without errors.');
  } catch (err) {
    console.error('Test harness failed:', err);
    process.exitCode = 1;
  }
})();
