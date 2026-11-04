// The row is reserved before contacting Discord. An uncertain send is reconciled
// from Discord history rather than automatically sending a second copy.
function createWarResultDelivery(execute) {
    return {
        async deliver(hwid, warId, deliverOrFind) {
            const insert = await execute("INSERT IGNORE INTO war_result_receipts (hwid, war_id, state) VALUES (?, ?, 'sending')", [hwid, warId]);
            const owner = insert.affectedRows === 1;
            if (!owner) {
                const rows = await execute('SELECT state, channel_id FROM war_result_receipts WHERE hwid=? AND war_id=?', [hwid, warId]);
                if (rows[0]?.state === 'delivered') return { success: true, duplicate: true, channelId: rows[0].channel_id };
            }
            try {
                const channelId = await deliverOrFind(owner);
                if (!channelId) return { success: false, pending: true, reason: 'delivery-unconfirmed' };
                await execute("UPDATE war_result_receipts SET state='delivered', channel_id=? WHERE hwid=? AND war_id=?", [channelId, hwid, warId]);
                return { success: true, channelId, duplicate: !owner };
            } catch (error) {
                if (owner && error.definitelyNotSent) await execute("DELETE FROM war_result_receipts WHERE hwid=? AND war_id=? AND state='sending'", [hwid, warId]);
                return { success: false, pending: !owner || !error.definitelyNotSent, reason: error.definitelyNotSent ? 'discord-unavailable' : 'delivery-unconfirmed' };
            }
        }
    };
}
module.exports = { createWarResultDelivery };
