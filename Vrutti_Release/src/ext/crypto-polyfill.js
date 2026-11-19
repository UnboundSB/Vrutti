if (!globalThis.crypto) {
    try {
        globalThis.crypto = require('crypto').webcrypto;
    } catch (e) {
        // Fallback
    }
}
