# sources.mk
# ----------

# +-- Set to 0 for English board output --+
CHINESE = 1

# +-- Add your own sources here, if any --+
ADD_SOURCES = alphabeta/cpp/alphabeta.cpp \
			  tt/cpp/transposition_table.cpp \
			  tt/cpp/zobrist.cpp