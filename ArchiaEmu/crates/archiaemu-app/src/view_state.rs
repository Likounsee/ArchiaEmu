#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Shared, explicit view states for frontend-neutral application pages.

/// A user-facing page state, independent of any specific UI toolkit.
///
/// Keeping absence, temporary loading, unavailable capability, and failure
/// distinct prevents frontends from collapsing materially different situations
/// into the same empty panel or success message.
#[derive(Clone, Debug, Eq, PartialEq)]
pub enum ViewState<T> {
    /// The page is waiting for an operation or initial data.
    Loading,
    /// Data is available and can be rendered.
    Ready(T),
    /// The operation succeeded, but there is no content to display.
    Empty,
    /// The requested operation cannot be offered in the current environment.
    Unavailable(String),
    /// The operation failed; the message should describe the failure.
    Error(String),
}

impl<T> ViewState<T> {
    /// Returns true when the state contains renderable data.
    pub const fn is_ready(&self) -> bool {
        matches!(self, Self::Ready(_))
    }

    /// Returns true when the operation completed successfully without content.
    pub const fn is_empty(&self) -> bool {
        matches!(self, Self::Empty)
    }

    /// Maps ready data while preserving every non-ready state unchanged.
    pub fn map<U>(self, map: impl FnOnce(T) -> U) -> ViewState<U> {
        match self {
            Self::Loading => ViewState::Loading,
            Self::Ready(value) => ViewState::Ready(map(value)),
            Self::Empty => ViewState::Empty,
            Self::Unavailable(reason) => ViewState::Unavailable(reason),
            Self::Error(message) => ViewState::Error(message),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::ViewState;

    #[test]
    fn page_states_remain_distinct_when_mapping_data() {
        let states = [
            ViewState::<u8>::Loading,
            ViewState::Empty,
            ViewState::Unavailable("audio device missing".to_owned()),
            ViewState::Error("preferences could not be saved".to_owned()),
        ];

        let expected = [
            ViewState::<String>::Loading,
            ViewState::Empty,
            ViewState::Unavailable("audio device missing".to_owned()),
            ViewState::Error("preferences could not be saved".to_owned()),
        ];

        for (state, expected) in states.into_iter().zip(expected) {
            let mapped = state.map(|value| value.to_string());
            assert!(!mapped.is_ready());
            assert_eq!(mapped, expected);
        }

        let ready = ViewState::Ready(42_u8).map(|value| value.to_string());
        assert_eq!(ready, ViewState::Ready("42".to_owned()));
    }

    #[test]
    fn empty_is_success_without_data_not_an_error() {
        let empty = ViewState::<Vec<String>>::Empty;
        assert!(empty.is_empty());
        assert!(!empty.is_ready());
        assert_eq!(empty, ViewState::Empty);
    }
}
