include(GNUInstallDirs)

set(GIT_VIEW_NAUTILUS_SCRIPTS
    nautilus/scripts/open-in-git_view
    nautilus/scripts/show-working-changes
    nautilus/scripts/commit-changes
    nautilus/scripts/show-log
    nautilus/scripts/fetch
    nautilus/scripts/pull
    nautilus/scripts/push
    nautilus/scripts/show-file-history
    nautilus/scripts/show-file-diff
)

install(
    PROGRAMS
        nautilus/run-git_view.sh
        nautilus/uri_to_path.py
        nautilus/nautilus-action.sh
    DESTINATION ${CMAKE_INSTALL_DATADIR}/git_view
)

install(
    PROGRAMS ${GIT_VIEW_NAUTILUS_SCRIPTS}
            nautilus/scripts/_git_view_action.sh
    DESTINATION ${CMAKE_INSTALL_DATADIR}/nautilus/scripts/GitView
)

install(
    FILES nautilus/git_view_nautilus.py
    DESTINATION ${CMAKE_INSTALL_DATADIR}/nautilus-python/extensions
)

install(
    FILES ${CMAKE_SOURCE_DIR}/packaging/git_view.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
