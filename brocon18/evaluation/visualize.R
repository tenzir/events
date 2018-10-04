#!/usr/bin/env Rscript --vanilla

suppressPackageStartupMessages(library(dplyr))
suppressPackageStartupMessages(library(ggplot2))
suppressPackageStartupMessages(library(scales))
suppressPackageStartupMessages(library(tidyr))

# -- global constants ----------------------------------------------------------

payload_levels <- as.integer(c(0, 10^(0:9)))
payload_labels <- c("0", "1", "10", "100", "1 KB", "10 KB", "100 KB", "1 MB",
                    "10 MB", "100 MB", "1 GB")
payload_length <- length(payload_levels)

payload_palette <- function(g) {
  d <- 360 / g
  h <- cumsum(c(15, rep(d, g - 1)))
  hcl(h = h, c = 100, l = 65)
}

payload_colors <- payload_palette(payload_length)

as.payload <- function(x) {
  factor(x, levels = payload_levels, labels = payload_labels)
}

# -- plot functions ------------------------------------------------------------

# Plots latency as a function of number of relays.
plot_latency <- function(data) {
  ggplot(data, aes(x = factor(relays), y = rtt,
                   shape = as.payload(payload),
                   group = as.payload(payload),
                   color = as.payload(payload))) +
    geom_line() +
    geom_point() +
    xlab("Relays") +
    labs(x = "Relays", group = "Payload", color = "Payload",
         shape = "Payload") +
    scale_color_manual(values = payload_colors) +
    scale_shape_manual(values=seq(0, payload_length)) +
    scale_y_continuous(name = "RTT (ms)", breaks = pretty_breaks(10),
                       labels = comma)
}

# -- main functions ------------------------------------------------------------

main <- function(args) {
  # Use a reasonable base font size.
  theme_set(theme_bw(base_size = 20))

  # Go through all files and plot the graphs.
  for (file in args) {
    data <- read.csv(file) %>%
      group_by(relays, payload) %>%
      summarize(rtt = median(rtt / 1e6))
    for (max_payload in 10^(6:8)) {
      filename <- paste(tools::file_path_sans_ext(file), max_payload, "pdf",
                        sep = ".")
      write(paste("-- generating", filename), stderr())
      data %>%
        filter(payload <= max_payload) %>%
        plot_latency %>%
        ggsave(filename = filename, height=9, width=16)
    }
  }
}

main(commandArgs(trailingOnly = TRUE))
