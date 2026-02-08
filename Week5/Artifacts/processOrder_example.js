const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

// Pseudo singletons shared across the app
const globalState = { lastOrderId: null, outageMode: false, retryCount: 0 };
const cache = { customer: new Map(), order: new Map() };
const logger = console;
const metrics = { inc: (name) => logger.debug(`[metric] ${name}`) };
const paymentGateway = {
  charge: (payload, cb) => setTimeout(() => cb(null, { authCode: crypto.randomBytes(2).toString('hex') }), 50)
};
const emailClient = {
  send: (opts, cb) => setTimeout(() => cb(null, { messageId: crypto.randomBytes(2).toString('hex') }), 10)
};
const auditWriter = fs;
const queue = { push: (job) => logger.debug(`[queue] enqueued ${job.type}`) };

function processOrder(orderId, overrides, callback) {
  if (typeof overrides === 'function') {
    callback = overrides;
    overrides = {};
  }
  overrides = overrides || {};
  callback = callback || function noop() {};

  metrics.inc('orders.process.start');
  globalState.lastOrderId = orderId;
  globalState.retryCount++;
  logger.info(`Processing order ${orderId} (retry #${globalState.retryCount})`);

  // Mutate caller-provided overrides to stash runtime info
  overrides._touchedAt = new Date();
  if (!overrides.meta) overrides.meta = {};
  overrides.meta.startedBy = overrides.meta.startedBy || process.env.USER || 'unknown';

  // Blocking config load from disk each call
  const configPath = overrides.configPath || process.env.LEGACY_ORDER_CONFIG || path.join(__dirname, 'order_config.json');
  let config = {};
  try {
    config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
  } catch (err) {
    logger.warn(`Falling back to defaults; could not read ${configPath}: ${err.message}`);
    config = { offlineMode: process.env.FORCE_OFFLINE === '1', auditPath: path.join(__dirname, 'audit.log') };
  }

  let customer = cache.customer.get(orderId);
  if (!customer) {
    customer = { id: orderId, status: 'guest', email: overrides.email || `${orderId}@example.com` };
    cache.customer.set(orderId, customer);
  }

  // Opportunistic order cache warm-up
  cache.order.set(orderId, { startedAt: Date.now(), flag: overrides.flag || 'standard' });

  // Hidden side-channel: write audit even before success
  try {
    auditWriter.appendFileSync(config.auditPath || path.join(__dirname, 'audit.log'), `START ${orderId}\n`);
  } catch (err) {
    logger.error(`Failed to append audit for ${orderId}: ${err.message}`);
  }

  // Legacy "service locator" flag that alters flow implicitly
  if (config.offlineMode || globalState.outageMode) {
    logger.warn('Running in offline mode; queuing order for later.');
    queue.push({ type: 'order.offline', orderId, payload: overrides });
    metrics.inc('orders.process.offline');
    callback(null, { status: 'queued', orderId });
    return;
  }

  paymentGateway.charge({ orderId, amount: overrides.amount || config.defaultAmount || 0 }, (err, auth) => {
    if (err) {
      logger.error(`Payment failed for ${orderId}: ${err.message}`);
      metrics.inc('orders.process.payment_error');
      queue.push({ type: 'order.retry', orderId, reason: 'payment', payload: overrides });
      return callback(err);
    }

    cache.order.set(orderId, { status: 'charged', auth });
    emailClient.send({ to: customer.email, template: overrides.template || 'receipt', context: { orderId, auth } }, (mailErr) => {
      if (mailErr) {
        logger.error(`Email failed for ${orderId}: ${mailErr.message}`);
        metrics.inc('orders.process.email_error');
      }

      setTimeout(() => {
        metrics.inc('orders.process.complete_delayed');
        cache.order.set(orderId, { status: 'completed', completedAt: Date.now() });
        try {
          auditWriter.appendFileSync(config.auditPath || path.join(__dirname, 'audit.log'), `DONE ${orderId}\n`);
        } catch (writeErr) {
          logger.error(`Audit completion write failed for ${orderId}: ${writeErr.message}`);
        }
      }, config.completionDelayMs || 200);

      callback(null, {
        orderId,
        auth,
        customer,
        flags: { outageMode: globalState.outageMode, retry: globalState.retryCount },
        metadata: overrides.meta,
      });
    });
  });
}

module.exports = { processOrder, globalState, cache };
