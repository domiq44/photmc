// photonmc.d.ts - TypeScript definitions for PhotonMC WASM API
// Generated from src/mc_wasm.c

export type ShapeType = 0 | 1 | 2;  // 0=SPHERE, 1=CUBE, 2=CYLINDER

export interface SimInput {
  /** Forme du nuage: 0=sphere, 1=cube, 2=cylinder */
  shape: ShapeType;
  /** Dimension caractéristique L_char (rayon sphère/cylindre, demi-arête cube) */
  size: number;
  /** Hauteur totale du cylindre (ignoré pour sphere/cube) */
  height: number;
  /** Épaisseur optique τ le long de L_char */
  tau: number;
  /** Angle d'incidence du faisceau en degrés [0, 90] */
  theta: number;
  /** Probabilité d'absorption par collision [0, 1] */
  absorb: number;
  /** Paramètre d'asymétrie composante 1 [-1, 1] */
  g1: number;
  /** Paramètre d'asymétrie composante 2 [-1, 1] */
  g2: number;
  /** Coefficient de mélange (poids de g1) [0, 1] */
  f: number;
  /** Nombre de photons incidents */
  nphotons: number;
  /** Nombre de bins angulaires de sortie */
  bins: number;
  /** Graine aléatoire 32 bits */
  seed: number;
  /** Étapes max par photon (sécurité boucle infinie) */
  maxsteps: number;
}

export interface SimOutput {
  /** Fraction de poids ressortie côté entrée (réfléchi) */
  reflected: number;
  /** Fraction de poids ressortie côté sortie (transmis) */
  transmitted: number;
  /** Fraction de poids absorbée = 1 - reflected - transmitted */
  absorbed: number;
  /** Poids total sortant = reflected + transmitted */
  out_weight: number;
  /** Nombre de photons effectivement injectés (ont touché le nuage) */
  photons_in: number;
  /** Nombre moyen de collisions par photon */
  mean_collisions: number;
  /** Profondeur moyenne de sortie (projection sur axe faisceau) */
  mean_depth: number;
  /** Nombre de bins angulaires */
  nbins: number;
  /** Taille histogramme collisions */
  max_collisions: number;
  /** Nombre de bins profondeur */
  ndepth: number;
  /** Poids moyen par bin angulaire [nbins] */
  hist_weight: number[];
  /** Nombre de photons par bin angulaire [nbins] */
  hist_count: number[];
  /** Nombre de photons par nombre de collisions [max_collisions] */
  hist_collisions: number[];
  /** Poids par bin de profondeur [ndepth] */
  hist_depth: number[];
}

/**
 * Exécute une simulation Monte Carlo unique.
 * @param input Paramètres de simulation
 * @returns Résultats de simulation (doit être libéré avec em_free)
 * @throws Error si allocation échoue
 */
export declare function em_run_simulation(
  shape: number, size: number, height: number, tau: number, theta: number,
  absorb: number, g1: number, g2: number, f: number,
  nphotons: number, bins: number, seed: number, maxsteps: number,
  out_size_ptr: number
): number;  // pointeur vers buffer double* dans heap WASM

/**
 * Lit le résultat depuis le buffer WASM après em_run_simulation.
 * @param ptr Pointeur retourné par em_run_simulation
 * @param out_size Taille du buffer en octets
 * @returns Objet SimOutput parsé
 */
export declare function read_sim_result(ptr: number, out_size: number): SimOutput;

/**
 * Libère un buffer alloué par em_run_simulation.
 * @param ptr Pointeur retourné par em_run_simulation
 */
export declare function em_free(ptr: number): void;

/**
 * Module Emscripten initialisé.
 */
export interface PhotonMCModule {
  cwrap: (ident: string, returnType: string, argTypes: string[]) => Function;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPF64: Float64Array;
  HEAP32: Int32Array;
}