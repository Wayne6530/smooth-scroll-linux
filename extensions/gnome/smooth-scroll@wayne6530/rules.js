// Pure rule model shared by the Shell runtime and GTK preferences.
export const RULES_TYPE = 'a(sssbua(bssbu))';
export const MATCH_MODES = ['contains', 'equals', 'starts-with', 'ends-with', 'regex'];

export function profileId(value) {
    return Number.isInteger(value) && value >= 0 && value <= 0xffffffff ? value : 0;
}

export function titleMatcher(rule) {
    if (!rule.title || !MATCH_MODES.includes(rule.mode))
        throw new Error('Enter a title condition and select a matching method.');
    if (rule.mode === 'regex') {
        const regex = new RegExp(rule.title, rule.ignore_case ? 'i' : '');
        return title => regex.test(title);
    }
    const fold = text => rule.ignore_case ? text.toLowerCase() : text;
    const needle = fold(rule.title);
    return title => {
        const value = fold(title);
        switch (rule.mode) {
        case 'contains': return value.includes(needle);
        case 'equals': return value === needle;
        case 'starts-with': return value.startsWith(needle);
        case 'ends-with': return value.endsWith(needle);
        default: return false;
        }
    };
}

export function normalizeRules(rules) {
    return rules.map(rule => ({
        ...rule,
        titles: rule.titles.map(title => {
            try {
                return {...title, matches: titleMatcher(title)};
            } catch (_error) {
                return {...title, matches: () => false};
            }
        }),
    }));
}

export function rulesFromVariant(value) {
    return value.map(([app, name, icon, enabled, profile_id, titles]) => ({
        app, name, icon, enabled, profile_id,
        titles: titles.map(([active, mode, title, ignore_case, id]) => ({
            enabled: active, mode, title, ignore_case, profile_id: id,
        })),
    }));
}

export function rulesToVariant(rules) {
    const apps = new Set();
    return rules.map(rule => {
        if (!rule.app || apps.has(rule.app))
            throw new Error('Each application must have exactly one rule.');
        apps.add(rule.app);
        if (profileId(rule.profile_id) !== rule.profile_id)
            throw new Error('Profile ID must be an integer between 0 and 4294967295.');
        return [rule.app, rule.name, rule.icon, rule.enabled, rule.profile_id,
            rule.titles.map(title => {
                titleMatcher(title);
                if (profileId(title.profile_id) !== title.profile_id)
                    throw new Error('Invalid title exception profile ID.');
                return [title.enabled, title.mode, title.title, title.ignore_case, title.profile_id];
            })];
    });
}

export function matchWindow(rules, info, defaultProfileId = 0) {
    if (info) {
        for (const rule of rules) {
            if (!rule.enabled || !info.appIds.includes(rule.app))
                continue;
            for (const title of rule.titles) {
                if (title.enabled && title.matches(info.title))
                    return {profileId: title.profile_id, condition: title.title};
            }
            return {profileId: rule.profile_id, condition: null};
        }
    }
    return {profileId: defaultProfileId, condition: null};
}

export function profileForWindow(rules, info, defaultProfileId = 0) {
    return matchWindow(rules, info, defaultProfileId).profileId;
}
