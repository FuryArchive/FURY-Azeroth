# Install FURY-owned SQL with the server artifact so production startup
# does not require the original Git checkout to remain next to the binaries.
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/modules/mod-fury/data/sql/fury"
    DESTINATION "share/fury/data/sql"
    COMPONENT fury-data
)
