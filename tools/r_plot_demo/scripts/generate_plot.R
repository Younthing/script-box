#!/usr/bin/env Rscript
suppressPackageStartupMessages({
  library(ggplot2)
})

parse_args <- function() {
  args <- commandArgs(trailingOnly = TRUE)
  params <- list(title = "Energy Tracking Example", points = 48L)
  for (item in args) {
    if (grepl("^--title=", item)) {
      params$title <- sub("^--title=", "", item)
    }
    if (grepl("^--points=", item)) {
      value <- as.integer(sub("^--points=", "", item))
      if (!is.na(value)) {
        params$points <- value
      }
    }
  }
  params$points <- max(12L, min(params$points, 360L))
  params
}

resolve_output_dir <- function() {
  env_dir <- Sys.getenv("TOOL_OUTPUT_DIR", unset = "")
  base <- if (nzchar(env_dir)) env_dir else file.path(getwd(), "outputs")
  dir.create(base, showWarnings = FALSE, recursive = TRUE)
  base
}

build_dataset <- function(points) {
  x <- seq_len(points)
  base <- 55 + sin(x / 5) * 6
  actual <- base + cos(x / 7) * 4 + sin(x / 3) * 2
  forecast <- base + 1.5
  band <- data.frame(Index = x, Low = base - 2, High = base + 2)
  lines <- rbind(
    data.frame(Index = x, Series = "Forecast", Value = forecast),
    data.frame(Index = x, Series = "Actual", Value = actual)
  )
  list(lines = lines, band = band)
}

main <- function() {
  params <- parse_args()
  dataset <- build_dataset(params$points)

  plot <- ggplot() +
    geom_ribbon(
      data = dataset$band,
      aes(x = Index, ymin = Low, ymax = High),
      fill = "#C6F6D5",
      alpha = 0.45
    ) +
    geom_line(
      data = dataset$lines,
      aes(x = Index, y = Value, color = Series),
      size = 1.2
    ) +
    scale_color_manual(values = c(Forecast = "#2F855A", Actual = "#DD6B20")) +
    labs(
      title = params$title,
      x = "Hour",
      y = "Load (kWh)",
      color = "Series"
    ) +
    theme_minimal(base_size = 12) +
    theme(legend.position = "top")

  output_dir <- resolve_output_dir()
  figure_path <- file.path(output_dir, "r_plot_demo.png")
  ggsave(filename = figure_path, plot = plot, width = 8, height = 4.5, dpi = 120)

  cat(sprintf("[r_plot_demo] Plot saved to %s (%d points)\n", figure_path, params$points))
}

main()
