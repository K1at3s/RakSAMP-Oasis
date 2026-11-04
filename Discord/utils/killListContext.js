const normalize = value => String(value || '').trim().toLowerCase();
function validateKillListContext(body, expectedGroup = '') {
    const own = normalize(body.ownGroup);
    const red = normalize(body.redName);
    const blue = normalize(body.blueName);
    if (!body.hwid || !body.playerName || typeof body.warId !== 'string' || !body.warId || body.wonKnown !== true) return 'missing-war-context';
    if (!own || !red || !blue || red === blue || (own !== red && own !== blue)) return 'wrong-war-teams';
    if (expectedGroup && normalize(expectedGroup) !== own) return 'wrong-bot-group';
    return null;
}
function createResultDeduplicator(now = Date.now) {
    const seen = new Map();
    return {
        reserve(hwid, warId) {
            for (const [key, time] of seen) if (now() - time > 86400000) seen.delete(key);
            const key = JSON.stringify([hwid, warId]);
            if (seen.has(key)) return false;
            if (seen.size >= 2000) seen.delete(seen.keys().next().value);
            seen.set(key, now());
            return true;
        },
        release(hwid, warId) { seen.delete(JSON.stringify([hwid, warId])); },
    };
}
module.exports = { validateKillListContext, createResultDeduplicator };
