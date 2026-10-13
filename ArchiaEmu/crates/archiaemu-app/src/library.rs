#![forbid(unsafe_code)]
#![deny(missing_docs)]

//! Lightweight, frontend-independent catalog of user-imported content.

use std::collections::BTreeMap;
use std::fmt;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

/// A content item that has been imported into the current host library.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ContentEntry {
    path: PathBuf,
    title: String,
    size_bytes: u64,
}

impl ContentEntry {
    /// Returns the canonical filesystem path to the content.
    pub fn path(&self) -> &Path {
        &self.path
    }

    /// Returns the filename-derived title shown in the library.
    pub fn title(&self) -> &str {
        &self.title
    }

    /// Returns the content file size captured when it was imported.
    pub const fn size_bytes(&self) -> u64 {
        self.size_bytes
    }
}

/// Errors returned while importing or removing a library entry.
#[derive(Debug)]
pub enum ContentLibraryError {
    /// Filesystem metadata or canonicalization failed.
    Io(io::Error),
    /// The path points to a directory or another non-file object.
    NotAFile(PathBuf),
    /// The file is empty and cannot be launched as content.
    Empty(PathBuf),
    /// The file exceeds the host's configured content-size limit.
    TooLarge {
        path: PathBuf,
        max_bytes: u64,
        actual_bytes: u64,
    },
    /// The canonical path is already present in the library.
    AlreadyImported(PathBuf),
    /// No matching entry exists in the library.
    NotImported(PathBuf),
}

impl fmt::Display for ContentLibraryError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(error) => write!(f, "content library filesystem error: {error}"),
            Self::NotAFile(path) => {
                write!(f, "content path is not a regular file: {}", path.display())
            }
            Self::Empty(path) => write!(f, "content file is empty: {}", path.display()),
            Self::TooLarge {
                path,
                max_bytes,
                actual_bytes,
            } => write!(
                f,
                "content file is too large: {} (maximum {max_bytes} bytes, got {actual_bytes})",
                path.display()
            ),
            Self::AlreadyImported(path) => {
                write!(f, "content is already imported: {}", path.display())
            }
            Self::NotImported(path) => {
                write!(f, "content is not in the library: {}", path.display())
            }
        }
    }
}

impl std::error::Error for ContentLibraryError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            Self::Io(error) => Some(error),
            _ => None,
        }
    }
}

/// An in-memory catalog that can be reused by terminal and desktop frontends.
///
/// Importing a path canonicalizes it, requires a regular non-empty file, and
/// enforces the caller's byte limit. Format detection and core-specific
/// validation still happen through the common launch pipeline.
#[derive(Default)]
pub struct ContentLibrary {
    entries: BTreeMap<PathBuf, ContentEntry>,
}

impl ContentLibrary {
    /// Creates an empty content catalog.
    pub fn new() -> Self {
        Self::default()
    }

    /// Returns imported entries in stable canonical-path order.
    pub fn entries(&self) -> impl Iterator<Item = &ContentEntry> {
        self.entries.values()
    }

    /// Returns the number of imported entries.
    pub fn len(&self) -> usize {
        self.entries.len()
    }

    /// Returns true when the catalog contains no entries.
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    /// Imports a file after validating its filesystem metadata and size.
    pub fn import(
        &mut self,
        path: impl AsRef<Path>,
        max_bytes: u64,
    ) -> Result<&ContentEntry, ContentLibraryError> {
        let canonical = fs::canonicalize(path.as_ref()).map_err(ContentLibraryError::Io)?;
        let metadata = fs::metadata(&canonical).map_err(ContentLibraryError::Io)?;
        if !metadata.is_file() {
            return Err(ContentLibraryError::NotAFile(canonical));
        }
        let size_bytes = metadata.len();
        if size_bytes == 0 {
            return Err(ContentLibraryError::Empty(canonical));
        }
        if size_bytes > max_bytes {
            return Err(ContentLibraryError::TooLarge {
                path: canonical,
                max_bytes,
                actual_bytes: size_bytes,
            });
        }
        if self.entries.contains_key(&canonical) {
            return Err(ContentLibraryError::AlreadyImported(canonical));
        }
        let title = canonical
            .file_stem()
            .or_else(|| canonical.file_name())
            .map(|name| name.to_string_lossy().into_owned())
            .filter(|name| !name.is_empty())
            .unwrap_or_else(|| canonical.display().to_string());
        let entry = ContentEntry {
            path: canonical.clone(),
            title,
            size_bytes,
        };
        self.entries.insert(canonical.clone(), entry);
        Ok(self
            .entries
            .get(&canonical)
            .expect("entry was just inserted"))
    }

    /// Removes an imported file by path; the file must still be present.
    pub fn remove(&mut self, path: impl AsRef<Path>) -> Result<ContentEntry, ContentLibraryError> {
        let canonical = fs::canonicalize(path.as_ref()).map_err(ContentLibraryError::Io)?;
        self.entries
            .remove(&canonical)
            .ok_or(ContentLibraryError::NotImported(canonical))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicUsize, Ordering};

    static NEXT_TEMP_ID: AtomicUsize = AtomicUsize::new(0);

    struct TempFile(PathBuf);

    impl TempFile {
        fn new(name: &str, bytes: &[u8]) -> Self {
            let id = NEXT_TEMP_ID.fetch_add(1, Ordering::Relaxed);
            let directory = std::env::temp_dir().join(format!(
                "archiaemu-library-{}-{id}",
                std::process::id()
            ));
            fs::create_dir(&directory).unwrap();
            let path = directory.join(name);
            fs::write(&path, bytes).unwrap();
            Self(path)
        }
    }

    impl Drop for TempFile {
        fn drop(&mut self) {
            let _ = fs::remove_file(&self.0);
            if let Some(parent) = self.0.parent() {
                let _ = fs::remove_dir(parent);
            }
        }
    }

    #[test]
    fn import_tracks_canonical_path_title_and_size() {
        let file = TempFile::new("zelda.gb", b"rom-data");
        let mut library = ContentLibrary::new();

        let entry = library.import(&file.0, 64).unwrap();
        assert_eq!(entry.path(), fs::canonicalize(&file.0).unwrap());
        assert_eq!(entry.title(), "zelda");
        assert_eq!(entry.size_bytes(), 8);
        assert_eq!(library.len(), 1);
    }

    #[test]
    fn import_rejects_duplicates_empty_files_and_oversized_files() {
        let file = TempFile::new("game.gb", b"1234");
        let empty = TempFile::new("empty.gb", b"");
        let mut library = ContentLibrary::new();

        library.import(&file.0, 4).unwrap();
        assert!(matches!(
            library.import(&file.0, 4),
            Err(ContentLibraryError::AlreadyImported(_))
        ));
        assert!(matches!(
            library.import(&empty.0, 4),
            Err(ContentLibraryError::Empty(_))
        ));
        assert!(matches!(
            library.import(&file.0, 3),
            Err(ContentLibraryError::TooLarge { .. })
        ));
    }

    #[test]
    fn remove_requires_an_imported_path_and_updates_the_catalog() {
        let file = TempFile::new("game.gb", b"rom");
        let mut library = ContentLibrary::new();

        assert!(matches!(
            library.remove(&file.0),
            Err(ContentLibraryError::NotImported(_))
        ));
        library.import(&file.0, 16).unwrap();
        assert_eq!(library.remove(&file.0).unwrap().title(), "game");
        assert!(library.is_empty());
    }

    #[test]
    fn import_rejects_directories() {
        let directory = std::env::temp_dir().join(format!(
            "archiaemu-library-dir-{}-{}",
            std::process::id(),
            NEXT_TEMP_ID.fetch_add(1, Ordering::Relaxed)
        ));
        fs::create_dir(&directory).unwrap();
        let mut library = ContentLibrary::new();

        assert!(matches!(
            library.import(&directory, 64),
            Err(ContentLibraryError::NotAFile(_))
        ));
        fs::remove_dir(directory).unwrap();
    }
}
