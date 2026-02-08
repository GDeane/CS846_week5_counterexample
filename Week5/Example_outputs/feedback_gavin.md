# Counterexample - Guideline 2: Constrain Summary Length

## 1. Rationale

The original guideline assumes that brevity improves clarity and reduces "fluff" (like error handling). 
However, in complex code such as legacy enterprise code, the "how" and the "edge cases" become vital pieces of information.

By strictly constraining the length to 20 words or less, the LLM is forced to aggresively abstract away complexity. This 
causes the model to omit implementation details such as code side-effects that are necessary for a developer to know.

Here I provide a counterexample where "Guideline 2: Explicitly Constrain Summary Length" would almost certainly fail to be 
optimal, as well as an updated guideline to better cover this case.

A prudent use case for using generative AI to summarize code is in understanding and debugging legacy code. This type of code is
often overloaded and side-effect heavy, meaning a long summary is necessary for correctness, safety, and understanding.

Constraining the summary length in this case can cause an oversimplified understanding of the code, leading to dangerous assumptions.

---

## 2. Example Problem

**Task Description:** Generate a clear and concise summary of the javascript processOrder function described in processOrder_example.js

> A Note on GenAI contribution: This "legacy code" and the test case for it was created using GitHub Copilot (GPT-5.1-Codex-Max)

```javascript
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


```

---

## 3. Documentation: Original Guideline Applied

**Prompt:**

> "Please generate a brief description in one sentence of the function processOrder (max 20 words).: [Code Snippet Above]"

**LLM Output:**

> `Processes an order by charging payment, sending email, updating caches and audit logs, and handling offline/retry flows.`

**Why this is sub-optimal:**
While this summary could be considered concise, it is **dangerous** due to it omitting several 'invisible' behaviors of
the above legacy code.

In reality, this is very "dirty" legacy code with many high-risk side effects, such as:
- the function quietly mutates the overrides object passed to it, something the developer may not know about.
- The function uses `fs.readFileSync` and `fs.appendFileSync`, which block the entire event loop in Node.js and will freeze
the server everytime it writes an audit log or reads the config.
- The function is **not stateless**. It modifies globalState when called, and its behavior depends on previous calls.

A good summary of this code should act as a warning label for developers that notes these kinds of obvious side effects.

---

## 4. Updated Guideline

**Guideline 2 (Revised): Content-Aware Length Scaling**

**Description:**

Ask the LLM to be clear and concise (high output information density), rather than imposing a hard word limit. 
Summaries should prioritize **functional completeness** over strict word counts for sensitive tasks.

**Reasoning:**

Strict constraints lead to over-generalization, where specific logic is replaced by vague verbs. This is dangerous
when the true complexity is much higher than a 20-word constraint would permit.

**Example:**

Summarize this function in a clear and concise manner (max 20 words). **Exception:** If the code includes non-standard
side-effects, you must add a 'Note' section highlighting each of these risks.


---

## 5. Documentation: Updated Guideline Applied

**Prompt:**

> "Summarize the processOrder function in a clear and concise manner (max 20 words). Exception: If the code includes non-standard
side-effects, you must add a 'Note' section highlighting each of these risks."

**LLM Output:**

Summary ✅
- Processes an order: loads config, charges payment, sends email, caches/audits, queues work, and returns via callback. (16 words)

`Note ⚠️:
- **Mutates global state (globalState):** updates lastOrderId and increments retryCount — can leak across calls/tests.
- **Mutates caller argument (overrides):** adds _touchedAt and meta.startedBy — unexpected external mutation.
- **Synchronous disk I/O (fs.readFileSync, appendFileSync):** blocks the event loop and may degrade performance.
- **Audit side-channel:** writes START before success and DONE later — can produce inconsistent or misleading audit records.
- **Delayed post-callback effects (setTimeout):** updates cache, metrics, and audit after callback returns — state changes occur asynchronously.
- **External side-effects:** calls paymentGateway.charge, emailClient.send, and queue.push — depends on/affects external systems and retries.
- **Environment-dependent behavior:** uses process.env and fallback config, altering behavior based on runtime environment.
`

**Result:**
The summary is longer, but it is closer to **optimal** because it captures the "hidden" behavior that a developer must 
know to avoid introducing further bugs. Any developer I know would prefer

---