package org.pjsip.glasses.voice

import android.content.Context

/**
 * Maps a spoken name ("warehouse", "support for this") to a SIP URI.
 * Local directory first, then CRM (the backend bridge's /contacts API),
 * then the AI bridge for fuzzy / vision-driven matches.
 */
object ContactResolver {

    data class Contact(val displayName: String, val uri: String)

    /* Demo directory; replace with CRM lookup via backend-bridge. */
    private val local = mapOf(
        "warehouse" to Contact("Warehouse", "sip:2001@pbx.local"),
        "support" to Contact("Support Desk", "sip:2100@pbx.local"),
        "dispatch" to Contact("Dispatch", "sip:2002@pbx.local"),
        "linphone" to Contact("Linphone peer", "sip:linphone@10.0.2.2:5062"),
        "pjsua" to Contact("pjsua peer", "sip:pjsua@10.0.2.2:5080"),
    )

    fun resolve(ctx: Context, spokenName: String): Contact? {
        val key = spokenName.lowercase().trim()
        local[key]?.let { return it }
        local.entries.firstOrNull { key.contains(it.key) }?.let { return it.value }
        return CrmClient.lookupByName(key)
    }

    fun normalize(raw: String): String = when {
        raw.startsWith("sip:") || raw.startsWith("sips:") -> raw
        raw.startsWith("tel:") -> "sip:${raw.removePrefix("tel:")}@pbx.local;user=phone"
        else -> "sip:$raw"
    }
}

/** Thin stub over backend-bridge GET /contacts?q= . */
object CrmClient {
    var baseUrl = "http://10.0.2.2:8787"
    fun lookupByName(name: String): ContactResolver.Contact? = null   // TODO HTTP
    fun lookupByUri(uri: String): String? = null                       // caller ID
}

/** Tracks the single active call id for voice/gesture commands. */
object CallSession {
    @Volatile var currentCallId: Int = -1
}
