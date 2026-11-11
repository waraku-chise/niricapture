.pragma library

var type = {
    "headlineSmall": { size: 24, weight: 500 },
    "titleLarge": { size: 20, weight: 500 },
    "titleMedium": { size: 16, weight: 500 },
    "bodyLarge": { size: 15, weight: 400 },
    "bodyMedium": { size: 13, weight: 400 },
    "labelLarge": { size: 14, weight: 500 },
    "labelMedium": { size: 12, weight: 500 }
}

var space = { "xs": 4, "sm": 8, "md": 12, "lg": 16, "xl": 24 }

function clamp(value, low, high) {
    return Math.max(low, Math.min(high, value))
}

function parseHex(hex) {
    if (typeof hex !== "string")
        return null
    var value = hex.trim()
    if (!/^#[0-9a-fA-F]{6}$/.test(value))
        return null
    return {
        "r": parseInt(value.substring(1, 3), 16),
        "g": parseInt(value.substring(3, 5), 16),
        "b": parseInt(value.substring(5, 7), 16)
    }
}

function toHex(rgb) {
    var r = clamp(Math.round(rgb.r), 0, 255).toString(16)
    var g = clamp(Math.round(rgb.g), 0, 255).toString(16)
    var b = clamp(Math.round(rgb.b), 0, 255).toString(16)
    if (r.length < 2) r = "0" + r
    if (g.length < 2) g = "0" + g
    if (b.length < 2) b = "0" + b
    return ("#" + r + g + b).toUpperCase()
}

function mix(a, b, amount) {
    var first = parseHex(a)
    var second = parseHex(b)
    if (!first || !second)
        return a
    return toHex({
        "r": first.r + (second.r - first.r) * amount,
        "g": first.g + (second.g - first.g) * amount,
        "b": first.b + (second.b - first.b) * amount
    })
}

function luminance(hex) {
    var rgb = parseHex(hex)
    if (!rgb)
        return 0
    return (0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b) / 255
}

function onColor(hex) {
    return luminance(hex) > 0.55 ? "#141922" : "#FFFFFF"
}

function roles(seedHex, baseHex) {
    var seed = parseHex(seedHex) ? seedHex : "#2F6FED"
    var base = parseHex(baseHex) ? baseHex : "#0F1116"
    var dark = luminance(base) < 0.5

    var surface = base
    var onSurface = dark ? "#EEF1F7" : mix("#000000", base, 0.18)
    var onSurfaceVariant = mix(onSurface, surface, dark ? 0.42 : 0.34)
    var outline = mix(onSurface, surface, dark ? 0.70 : 0.62)
    var outlineVariant = mix(onSurface, surface, dark ? 0.85 : 0.82)

    var primary = dark ? mix(seed, "#FFFFFF", 0.45) : mix(seed, "#000000", 0.15)
    var onPrimary = onColor(primary)
    var primaryContainer = dark ? mix(seed, surface, 0.55) : mix(seed, "#FFFFFF", 0.78)
    var onPrimaryContainer = dark ? mix(seed, "#FFFFFF", 0.72) : mix(seed, "#000000", 0.35)
    var secondaryContainer = dark ? mix(seed, surface, 0.72) : mix(seed, "#FFFFFF", 0.86)
    var onSecondaryContainer = dark ? mix(seed, "#FFFFFF", 0.62) : mix(seed, "#000000", 0.42)

    var surfaceContainerLow = dark ? mix(surface, "#FFFFFF", 0.04) : mix(surface, "#000000", 0.02)
    var surfaceContainer = dark ? mix(surface, "#FFFFFF", 0.07) : mix(surface, "#000000", 0.035)
    var surfaceContainerHigh = dark ? mix(surface, "#FFFFFF", 0.10) : mix(surface, "#000000", 0.05)
    var surfaceContainerHighest = dark ? mix(surface, "#FFFFFF", 0.14) : mix(surface, "#000000", 0.07)

    var error = dark ? "#FFB4AB" : "#BA1A1A"
    var onError = onColor(error)
    var errorContainer = dark ? "#93000A" : "#FFDAD6"
    var onErrorContainer = dark ? "#FFDAD6" : "#410002"

    return {
        "dark": dark,
        "surface": surface,
        "onSurface": onSurface,
        "onSurfaceVariant": onSurfaceVariant,
        "outline": outline,
        "outlineVariant": outlineVariant,
        "primary": primary,
        "onPrimary": onPrimary,
        "primaryContainer": primaryContainer,
        "onPrimaryContainer": onPrimaryContainer,
        "secondaryContainer": secondaryContainer,
        "onSecondaryContainer": onSecondaryContainer,
        "surfaceContainerLow": surfaceContainerLow,
        "surfaceContainer": surfaceContainer,
        "surfaceContainerHigh": surfaceContainerHigh,
        "surfaceContainerHighest": surfaceContainerHighest,
        "error": error,
        "onError": onError,
        "errorContainer": errorContainer,
        "onErrorContainer": onErrorContainer,
        "hover": mix(surface, onSurface, 0.08),
        "pressed": mix(surface, onSurface, 0.12),
        "scrim": "#000000"
    }
}
