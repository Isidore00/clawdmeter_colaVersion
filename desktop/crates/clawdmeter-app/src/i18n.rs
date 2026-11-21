//! Interface language. English text in the source is the key: [`tr`] looks it
//! up in the Traditional Chinese (Taiwan) table and returns the English text
//! unchanged when the language is English or the table has no entry (a
//! dynamic string, a path). Strings with numbers or names in them are written
//! as a template with `{1}`, `{2}` placeholders and filled by [`trf`], so the
//! word order is the translation's own.
//!
//! The language is process-wide: [`apply`] sets it from the `language`
//! setting (`auto` follows the operating system). It starts as English, so
//! tests that look at text don't depend on the machine they run on.

use std::collections::{HashMap, HashSet};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{LazyLock, Mutex, OnceLock};

use clawdmeter_core::config::Language;
use gpui::SharedString;

mod zh_tw;

static ZH: AtomicBool = AtomicBool::new(false);

static TABLE: LazyLock<HashMap<&'static str, &'static str>> = LazyLock::new(|| zh_tw::TABLE.iter().copied().collect());

/// Set the language: the setting's choice, or the OS's for `auto`.
pub fn apply(language: Language) {
    let zh = match language {
        Language::En => false,
        Language::ZhTw => true,
        Language::Auto => system_is_zh_tw(),
    };
    ZH.store(zh, Ordering::Relaxed);
}

/// Read the `language` setting and apply it (the tray and the window call
/// this at start-up).
pub fn init(paths: &clawdmeter_core::Paths) {
    apply(clawdmeter_core::Settings::load(paths).map(|s| s.language).unwrap_or_default());
}

/// The operating system asks for Traditional Chinese (asked once). Tests
/// always get English, whatever machine they run on.
fn system_is_zh_tw() -> bool {
    if cfg!(test) {
        return false;
    }
    static SYSTEM: OnceLock<bool> = OnceLock::new();
    *SYSTEM.get_or_init(|| clawdmeter_core::platform::system_locale().is_some_and(|l| Language::locale_is_zh_tw(&l)))
}

/// Is the interface in Traditional Chinese right now?
pub fn is_zh() -> bool {
    ZH.load(Ordering::Relaxed)
}

/// The translation of `text`, or `text` itself.
pub fn tr(text: impl AsRef<str>) -> SharedString {
    translate(is_zh(), text.as_ref())
}

fn translate(zh: bool, text: &str) -> SharedString {
    if !zh {
        return SharedString::from(text.to_owned());
    }
    if let Some(zh) = TABLE.get(text) {
        return SharedString::from(*zh);
    }
    match translate_dynamic(text) {
        Some(zh) => SharedString::from(zh),
        None => {
            note_missing(text);
            SharedString::from(text.to_owned())
        }
    }
}

/// An entry of the table with `{1}`, `{2}` … in it, split at the
/// placeholders: the literal pieces, and the translation.
struct Pattern {
    literals: Vec<&'static str>,
    zh: &'static str,
}

static PATTERNS: LazyLock<Vec<Pattern>> = LazyLock::new(|| {
    zh_tw::TABLE
        .iter()
        .filter(|(en, _)| en.contains("{1}"))
        .map(|(en, zh)| {
            let mut literals = Vec::new();
            let mut rest = *en;
            while let Some(i) = ["{1}", "{2}", "{3}", "{4}"].iter().filter_map(|p| rest.find(p)).min() {
                literals.push(&rest[..i]);
                rest = &rest[i + 3..];
            }
            literals.push(rest);
            Pattern { literals, zh }
        })
        .collect()
});

/// What the placeholders of `p` stand for in `text`, if it is that sentence.
fn captures<'a>(p: &Pattern, text: &'a str) -> Option<Vec<&'a str>> {
    let (first, last) = (p.literals.first()?, p.literals.last()?);
    if !text.starts_with(first) || text.len() < first.len() + last.len() || !text.ends_with(last) {
        return None;
    }
    let end = text.len() - last.len();
    let mut at = first.len();
    let mut found = Vec::new();
    for lit in &p.literals[1..p.literals.len() - 1] {
        let i = at + text[at..end].find(lit)?;
        found.push(&text[at..i]);
        at = i + lit.len();
    }
    found.push(&text[at..end]);
    found.iter().all(|c| !c.is_empty()).then_some(found)
}

/// Sentences built elsewhere with `format!` (core's error messages, a path
/// in the middle): matched against the templates, and a message with a retry
/// line is translated in two parts.
fn translate_dynamic(text: &str) -> Option<String> {
    // The most specific sentence wins ("Firmware v{1}" also fits the longer
    // "Firmware v{1} · v{2} available"); what stands in a placeholder is
    // translated too (an error's reason, a place name).
    let best = PATTERNS
        .iter()
        .filter_map(|p| captures(p, text).map(|c| (p, c)))
        .max_by_key(|(p, _)| p.literals.iter().map(|l| l.len()).sum::<usize>());
    if let Some((p, c)) = best {
        let c: Vec<String> = c.iter().map(|c| lookup(c).unwrap_or_else(|| (*c).to_owned())).collect();
        let c: Vec<&str> = c.iter().map(String::as_str).collect();
        return Some(fill(p.zh, &c));
    }
    // "480×480 · 3 buttons": a board's summary, piece by piece.
    if text.contains(" · ") {
        let parts: Vec<String> = text.split(" · ").map(|p| lookup(p).unwrap_or_else(|| p.to_owned())).collect();
        let joined = parts.join(" · ");
        if joined != text {
            return Some(joined);
        }
    }
    for marker in [" Trying again in about ", " Retrying."] {
        if let Some(i) = text.find(marker) {
            let (head, tail) = (&text[..i], &text[i + 1..]);
            if let (Some(h), Some(t)) = (lookup(head), lookup(tail)) {
                return Some(format!("{h}{t}"));
            }
        }
    }
    None
}

fn lookup(text: &str) -> Option<String> {
    TABLE.get(text).map(|s| (*s).to_owned()).or_else(|| translate_dynamic(text))
}

/// [`tr`] for a template: `{1}`, `{2}` … become `args`.
pub fn trf(template: &'static str, args: &[&str]) -> String {
    fill(&tr(template), args)
}

fn fill(template: &str, args: &[&str]) -> String {
    let mut out = template.to_owned();
    for (i, a) in args.iter().enumerate() {
        out = out.replace(&format!("{{{}}}", i + 1), a);
    }
    out
}

/// With `CLAWDMETER_I18N_DEBUG` set, log every English string the Chinese
/// table lacked once (dynamic strings show up here too).
fn note_missing(text: &str) {
    static ON: LazyLock<bool> = LazyLock::new(|| std::env::var_os("CLAWDMETER_I18N_DEBUG").is_some());
    static SEEN: LazyLock<Mutex<HashSet<String>>> = LazyLock::new(Mutex::default);
    if *ON && text.chars().any(|c| c.is_ascii_alphabetic()) {
        if let Ok(mut seen) = SEEN.lock() {
            if seen.insert(text.to_owned()) {
                log::warn!("i18n: no zh-TW text for {text:?}");
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn table_has_no_duplicate_keys_and_keeps_placeholders() {
        let mut seen = HashSet::new();
        for (en, zh) in zh_tw::TABLE {
            assert!(seen.insert(*en), "duplicate key {en:?}");
            for n in 1..=4 {
                let p = format!("{{{n}}}");
                assert_eq!(en.contains(&p), zh.contains(&p), "{en:?}: placeholder {p} must appear in both");
            }
        }
    }

    #[test]
    fn english_passes_through_and_templates_fill() {
        let t = |zh, s: &'static str| translate(zh, s).to_string();
        assert_eq!(t(false, "Overview"), "Overview");
        assert_eq!(t(true, "Overview"), "總覽");
        assert_eq!(t(true, "not in the table"), "not in the table");
        assert_eq!(fill(&t(true, "{1} min ago"), &["5"]), "5 分鐘前");
        assert_eq!(fill(&t(false, "{1} min ago"), &["5"]), "5 min ago");
    }
}
