#!/usr/bin/env Rscript
suppressPackageStartupMessages({
  library(tibble)
  library(dplyr)
})

parse_args <- function() {
  args <- commandArgs(trailingOnly = TRUE)
  params <- list(rows = 6L, focus_region = "north")
  for (item in args) {
    if (grepl("^--rows=", item)) {
      value <- as.integer(sub("^--rows=", "", item))
      if (!is.na(value)) {
        params$rows <- value
      }
    }
    if (grepl("^--focus_region=", item)) {
      raw <- sub("^--focus_region=", "", item)
      params$focus_region <- tolower(raw)
    }
  }
  params$rows <- max(4L, min(params$rows, 80L))
  valid_regions <- c("north", "east", "south", "west")
  if (!(params$focus_region %in% valid_regions)) {
    params$focus_region <- "north"
  }
  params
}

resolve_output_dir <- function() {
  env_dir <- Sys.getenv("TOOL_OUTPUT_DIR", unset = "")
  base <- if (nzchar(env_dir)) env_dir else file.path(getwd(), "outputs")
  dir.create(base, showWarnings = FALSE, recursive = TRUE)
  base
}

main <- function() {
  settings <- parse_args()
  rows <- settings$rows
  focus <- settings$focus_region

  region_lookup <- c(north = "North", east = "East", south = "South", west = "West")
  set.seed(20241128)
  tbl <- tibble(
    Region = rep(names(region_lookup), length.out = rows),
    Division = sample(c("Analytics", "Finance", "Operations", "Sales"), rows, replace = TRUE),
    Projects = sample(3:14, rows, replace = TRUE),
    Score = round(runif(rows, min = 68, max = 99), 1)
  ) |>
    mutate(
      Rank = dense_rank(desc(Score)),
      Region = unname(region_lookup[Region]),
      Highlight = if_else(tolower(Region) == focus, "*", "")
    ) |>
    arrange(Rank) |>
    select(Rank, Highlight, Region, Division, Projects, Score)

  output_dir <- resolve_output_dir()
  csv_path <- file.path(output_dir, "r_table_demo.csv")
  write.csv(tbl, csv_path, row.names = FALSE)

  display_tbl <- tbl |>
    mutate(
      Score = sprintf("%.1f", Score)
    )

  cat(sprintf("[r_table_demo] Regional delivery table (%d rows)\n", nrow(display_tbl)))
  print(display_tbl, row.names = FALSE)
  cat(sprintf("[r_table_demo] Table saved to %s\n", csv_path))
}

main()
