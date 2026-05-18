/*
  StreamIMDb TV App — Backend Server
  ===================================
  Stack: Node.js + Express
  Provides:
    GET  /api/movies          → full movie catalogue
    GET  /api/movies/:imdbId  → single movie detail + stream URL
    GET  /api/stream/:imdbId  → resolves the HLS stream manifest URL
    GET  /api/health          → uptime check

  Run:
    npm install
    node server.js
  Listens on http://localhost:3000
*/

import express from "express";
import cors from "cors";

const app = express();
const PORT = process.env.PORT || 3000;

app.use(cors());
app.use(express.json());

/* ─── Movie Catalogue ────────────────────────────────────────────────── */

const MOVIES = [
  {
    imdbId: "tt0468569",
    title: "The Dark Knight",
    year: 2008,
    rating: 9.0,
    genre: ["Action", "Crime", "Drama"],
    director: "Christopher Nolan",
    runtime: 152,
    synopsis:
      "When the menace known as the Joker wreaks havoc and chaos on the people of Gotham, Batman must accept one of the greatest psychological and physical tests of his ability to fight injustice.",
    posterColor: "#1E2840",
    accentColor: "#E59320",
  },
  {
    imdbId: "tt1375666",
    title: "Inception",
    year: 2010,
    rating: 8.8,
    genre: ["Action", "Adventure", "Sci-Fi"],
    director: "Christopher Nolan",
    runtime: 148,
    synopsis:
      "A thief who steals corporate secrets through the use of dream-sharing technology is given the inverse task of planting an idea into the mind of a C.E.O.",
    posterColor: "#1A3020",
    accentColor: "#4CAF50",
  },
  {
    imdbId: "tt0816692",
    title: "Interstellar",
    year: 2014,
    rating: 8.7,
    genre: ["Adventure", "Drama", "Sci-Fi"],
    director: "Christopher Nolan",
    runtime: 169,
    synopsis:
      "A team of explorers travel through a wormhole in space in an attempt to ensure humanity's survival.",
    posterColor: "#2A1010",
    accentColor: "#FF6B35",
  },
  {
    imdbId: "tt0110912",
    title: "Pulp Fiction",
    year: 1994,
    rating: 8.9,
    genre: ["Crime", "Drama"],
    director: "Quentin Tarantino",
    runtime: 154,
    synopsis:
      "The lives of two mob hitmen, a boxer, a gangster and his wife, and a pair of diner bandits intertwine in four tales of violence and redemption.",
    posterColor: "#2A2010",
    accentColor: "#FFD700",
  },
  {
    imdbId: "tt0137523",
    title: "Fight Club",
    year: 1999,
    rating: 8.8,
    genre: ["Drama"],
    director: "David Fincher",
    runtime: 139,
    synopsis:
      "An insomniac office worker and a devil-may-care soap maker form an underground fight club that evolves into much more.",
    posterColor: "#1C1010",
    accentColor: "#CC3300",
  },
  {
    imdbId: "tt0133093",
    title: "The Matrix",
    year: 1999,
    rating: 8.7,
    genre: ["Action", "Sci-Fi"],
    director: "Lana Wachowski",
    runtime: 136,
    synopsis:
      "A computer hacker learns from mysterious rebels about the true nature of his reality and his role in the war against its controllers.",
    posterColor: "#0A1A0A",
    accentColor: "#00FF41",
  },
];

/*
  In a real app this would call a streaming rights API / CDN resolver.
  We use a reliable public HLS demo stream for all titles.
*/
const DEMO_STREAM_URL =
  "https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.m3u8";

/* ─── Routes ─────────────────────────────────────────────────────────── */

app.get("/api/health", (_req, res) => {
  res.json({ status: "ok", uptime: process.uptime() });
});

app.get("/api/movies", (_req, res) => {
  res.json({ movies: MOVIES });
});

app.get("/api/movies/:imdbId", (req, res) => {
  const movie = MOVIES.find((m) => m.imdbId === req.params.imdbId);
  if (!movie) return res.status(404).json({ error: "Movie not found" });
  res.json(movie);
});

app.get("/api/stream/:imdbId", (req, res) => {
  const movie = MOVIES.find((m) => m.imdbId === req.params.imdbId);
  if (!movie) return res.status(404).json({ error: "Movie not found" });

  res.json({
    imdbId: movie.imdbId,
    title: movie.title,
    streamUrl: DEMO_STREAM_URL,
    format: "hls",
    drm: null,
  });
});

/* ─── Start ──────────────────────────────────────────────────────────── */

app.listen(PORT, () => {
  console.log(`StreamIMDb API running → http://localhost:${PORT}`);
  console.log(`  GET /api/movies`);
  console.log(`  GET /api/movies/:imdbId`);
  console.log(`  GET /api/stream/:imdbId`);
});
