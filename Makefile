# Makefile - Simulateur Monte Carlo de transport de photons
#
# Cibles principales :
#   make            -> compile le binaire ./photonmc (rapide)
#   make compile    -> idem (alias)
#   make run        -> execution simple (sphere, tau=1, theta=0)
#   make scans      -> genere les balayages theta/tau + distribution angulaire (out/)
#   make plots      -> genere tous les PNG gnuplot dans out/
#   make scans_formes -> 3 scans (sphere/cube/cylinder) dans out/sphere, out/cube, out/cylinder
#   make plots_formes -> comparaison des formes (comparaison_formes_*.png)
#   make all        -> compile + scans + plots + scans_formes + plots_formes
#   make clean      -> nettoie objets/binaire
#   make distclean  -> nettoie aussi les sorties
#   make serve      -> lance l'interface web locale (http://localhost:8000/)
#   make test       -> tests de validation
#   make wasm       -> compile WebAssembly (Emscripten)
#   make format     -> formatage code avec clang-format
#   make tidy       -> analyse statique avec clang-tidy
#   make asan       -> compilation avec AddressSanitizer + UBSan
#   make compdb     -> genere compile_commands.json (pour LSP/clangd)
#   make bench      -> benchmark performance

CC      := gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
           -Wdouble-promotion -I src
LDFLAGS := -lm

# Flags pour AddressSanitizer + UndefinedBehaviorSanitizer
ASAN_CFLAGS  := -std=c11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
                -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -I src
ASAN_LDFLAGS := -lm -fsanitize=address,undefined

SRC_DIR := src
OBJ_DIR := obj
BIN     := photonmc

SRCS := $(SRC_DIR)/rng.c \
        $(SRC_DIR)/geometry.c \
        $(SRC_DIR)/phase.c \
        $(SRC_DIR)/mc.c \
        $(SRC_DIR)/output.c \
        $(SRC_DIR)/main.c
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

OUTDIR := out

# Parametres de simulation par defaut (surchargeables : make NP=... )
NP     ?= 400000
BINS   ?= 180
SEED   ?= 12345

.PHONY: all run scans plots scans_formes plots_formes serve clean distclean \
        compile test wasm format tidy asan compdb bench

# Cible par defaut : compilation rapide (pas de simulations).
compile: $(BIN)

# Tests de validation (reproductibilite + coherence physique + convergence + reference analytique + cas limites + validation analytique).
test: $(BIN)
	@echo "=== Tests de photonmc ==="
	@bash tests/test_photonmc.sh ./photonmc
	@echo
	@echo "=== Test de reference analytique ==="
	@bash tests/test_reference.sh ./photonmc 0.01
	@echo
	@echo "=== Test de convergence ==="
	@bash tests/test_convergence.sh ./photonmc 0.05
	@echo
	@echo "=== Test des formes ==="
	@bash tests/test_shapes.sh ./photonmc
	@echo
	@echo "=== Test cas limites ==="
	@bash tests/test_edge_cases.sh ./photonmc
	@echo
	@echo "=== Validation analytique ==="
	@bash tests/test_analytical.sh ./photonmc

all: compile scans plots scans_formes plots_formes

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# --- executions -------------------------------------------------------------

run: $(BIN)
	@mkdir -p $(OUTDIR)
	./$(BIN) --shape sphere --tau 1.0 --theta 0 --absorb 0.0 \
	    --g1 0.0 --g2 0.0 --f 0.5 --nphotons $(NP) --bins $(BINS) --seed $(SEED)

# Balayage en angle d'incidence (cube : dependance angulaire non triviale)
scans: $(BIN)
	@mkdir -p $(OUTDIR)
	./$(BIN) --scan theta --from 0 --to 80 --scan-steps 30 \
	    --shape cube --size 1 --tau 3.0 --absorb 0.05 --g1 0.8 --g2 -0.3 --f 0.7 \
	    --nphotons $(NP) --bins $(BINS) --seed $(SEED)
	./$(BIN) --scan tau --from 0.1 --to 20 --scan-steps 40 \
	    --shape sphere --theta 30 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7 \
	    --nphotons $(NP) --bins $(BINS) --seed $(SEED)
	./$(BIN) --shape sphere --tau 2.0 --theta 0 --absorb 0.1 \
	    --g1 0.8 --g2 -0.3 --f 0.7 --nphotons $(NP) --bins $(BINS) --seed $(SEED)
	@echo "Balayages ecrits dans $(OUTDIR)/"

# 3 scans pour la comparaison des formes (sphere/cube/cylinder) -> outdir distincts
scans_formes: $(BIN)
	@mkdir -p out/sphere out/cube out/cylinder
	./$(BIN) --scan tau --from 0.1 --to 20 --scan-steps 40 \
	    --shape sphere --theta 30 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7 \
	    --nphotons $(NP) --bins $(BINS) --seed $(SEED) --outdir out/sphere
	./$(BIN) --scan tau --from 0.1 --to 20 --scan-steps 40 \
	    --shape cube --theta 30 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7 \
	    --nphotons $(NP) --bins $(BINS) --seed $(SEED) --outdir out/cube
	./$(BIN) --scan tau --from 0.1 --to 20 --scan-steps 40 \
	    --shape cylinder --theta 30 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7 \
	    --nphotons $(NP) --bins $(BINS) --seed $(SEED) --outdir out/cylinder
	@echo "Scans formes ecrits dans out/sphere, out/cube, out/cylinder/"

# --- graphiques gnuplot ------------------------------------------------------

plots: $(BIN) scans
	gnuplot scripts/plot_flux_theta.gp
	gnuplot scripts/plot_flux_tau.gp
	gnuplot scripts/plot_angular.gp
	gnuplot scripts/plot_phase.gp
	gnuplot scripts/plot_angular_log.gp
	gnuplot scripts/plot_angular_sr.gp
	gnuplot scripts/plot_angular_costheta.gp
	gnuplot scripts/plot_bilan_theta.gp
	gnuplot scripts/plot_bilan_tau.gp
	gnuplot scripts/plot_rapport_rt.gp
	gnuplot scripts/plot_hist_collisions.gp
	gnuplot scripts/plot_profondeur.gp
	@echo "Graphiques ecrits dans $(OUTDIR)/"

plots_formes: scans_formes
	gnuplot scripts/plot_comparaison_formes.gp
	@echo "Graphiques de comparaison des formes ecrits dans $(OUTDIR)/"

# Lance l'interface web locale (http://localhost:8000/)
serve: $(BIN)
	@mkdir -p $(OUTDIR)
	python3 web/server.py --bin ./$(BIN) --outdir $(OUTDIR)

# --- WebAssembly (Emscripten) -------------------------------------------------

WASM_OUT := web/photonmc.js
WASM_WASM := web/photonmc.wasm
WASM_SRCS := $(SRC_DIR)/mc_wasm.c $(SRC_DIR)/mc.c $(SRC_DIR)/geometry.c \
             $(SRC_DIR)/phase.c $(SRC_DIR)/rng.c $(SRC_DIR)/output.c

wasm:
	@if command -v emcc >/dev/null 2>&1; then \
		mkdir -p web/config; \
		cp config/presets.json web/config/presets.json; \
		emcc -O2 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 \
			-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","getValue","setValue"]' \
			-s EXPORTED_FUNCTIONS='["_em_run_simulation","_em_free","_malloc","_free"]' \
			-s MODULARIZE=1 -s EXPORT_NAME="PhotonMCModule" \
			-I src $(WASM_SRCS) -o $(WASM_OUT); \
		echo "WebAssembly compile dans $(WASM_OUT) et $(WASM_WASM)"; \
		echo "Presets copiés dans web/config/"; \
	else \
		echo "Emscripten (emcc) non installe."; \
		echo "  git clone https://github.com/emscripten-core/emsdk.git"; \
		echo "  cd emsdk && ./emsdk install latest && ./emsdk activate latest"; \
		exit 1; \
	fi

# --- Outils developpement -----------------------------------------------------

# Formattage code avec clang-format
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		clang-format -i src/*.c src/*.h; \
		echo "Code formate avec clang-format"; \
	else \
		echo "clang-format non installe (apt install clang-format)"; \
		exit 1; \
	fi

# Analyse statique avec clang-tidy
tidy:
	@if command -v clang-tidy >/dev/null 2>&1; then \
		clang-tidy -p . src/*.c -- -I src -std=c11; \
	else \
		echo "clang-tidy non installe (apt install clang-tidy)"; \
		echo "Requiert compile_commands.json (make compdb)"; \
		exit 1; \
	fi

# Compilation avec AddressSanitizer + UndefinedBehaviorSanitizer
ASAN_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.asan.o,$(SRCS))
ASAN_BIN  := photonmc-asan

$(OBJ_DIR)/%.asan.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(ASAN_CFLAGS) -c $< -o $@

$(ASAN_BIN): $(ASAN_OBJS)
	$(CC) $(ASAN_OBJS) -o $@ $(ASAN_LDFLAGS)

asan: $(ASAN_BIN)
	@echo "Binaire ASan: ./$(ASAN_BIN)"
	@echo "Exemple: ASAN_OPTIONS=detect_leaks=1 ./$(ASAN_BIN) --nphotons 10000 --tau 5"

# Generation compile_commands.json pour LSP/clangd (necessite bear)
compdb:
	@if command -v bear >/dev/null 2>&1; then \
		$(MAKE) clean; \
		bear -- $(MAKE) compile; \
		echo "compile_commands.json genere"; \
	else \
		echo "bear non installe (apt install bear)"; \
		echo "Alternative: pip install compile-commands-generator"; \
		exit 1; \
	fi

# Benchmark performance (necessite hyperfine)
bench: $(BIN)
	@if command -v hyperfine >/dev/null 2>&1; then \
		hyperfine --warmup 3 --runs 10 \
			'./$(BIN) --nphotons 100000 --tau 2 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7' \
			'./$(BIN) --nphotons 100000 --tau 5 --absorb 0.0 --g1 0.0 --g2 0.0 --f 0.5' \
			'./$(BIN) --nphotons 100000 --tau 10 --absorb 0.2 --g1 0.9 --g2 -0.5 --f 0.8'; \
	else \
		echo "hyperfine non installe (cargo install hyperfine / apt install hyperfine)"; \
		echo "Fallback: time ./$(BIN) --nphotons 100000 --tau 2 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7"; \
		time ./$(BIN) --nphotons 100000 --tau 2 --absorb 0.1 --g1 0.8 --g2 -0.3 --f 0.7 > /dev/null; \
	fi

clean:
	rm -rf $(OBJ_DIR) $(BIN) $(WASM_OUT) $(WASM_WASM) $(ASAN_BIN) web/config/presets.json web/__pycache__

distclean: clean
	rm -rf $(OUTDIR) compile_commands.json