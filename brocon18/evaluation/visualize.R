#!/usr/bin/env Rscript --vanilla

suppressPackageStartupMessages(library(dplyr))
suppressPackageStartupMessages(library(ggplot2))
suppressPackageStartupMessages(library(scales))
suppressPackageStartupMessages(library(tidyr))

# -- plot functions ------------------------------------------------------------

payload_palette <- function(g) {
  d <- 360 / g
  h <- cumsum(c(15, rep(d, g - 1)))
  hcl(h = h, c = 100, l = 65)
}

# Plots data as a function of number of relays. Each line represents a
# different payload.
plot_data <- function(data, y) {
  si_labels <- c("0", "1", "10", "100", "1 KB", "10 KB", "100 KB", "1 MB",
                 "10 MB", "100 MB", "1 GB")
  stopifnot(length(si_labels) >= length(levels(data$payload)))
  ggplot(data, aes_(x = quote(factor(relays)), y = as.name(y),
                    shape = quote(payload),
                    group = quote(payload),
                    color = quote(payload))) +
    geom_line() +
    geom_point() +
    xlab("Relays") +
    labs(group = "Payload", color = "Payload", shape = "Payload") +
    scale_color_manual(values = payload_palette(length(si_labels)),
                       labels = si_labels) +
    scale_shape_manual(values = seq(0, length(levels(data$payload))),
                       labels = si_labels) +
    scale_y_continuous(breaks = pretty_breaks(10), labels = comma)
}

plot_latency <- function(data) {
  plot_data(data, "rtt") +
    ylab("RTT (ms)")
}

plot_throughput <- function(data) {
  plot_data(data, "throughput") +
    ylab("Throughput (messages/sec)")
}

plot_throughput_boxplots <- function(data) {
  ggplot(data, aes(payload, messages, color = payload)) +
    geom_boxplot() +
    scale_y_continuous(breaks = pretty_breaks(5), labels = comma) +
    labs(x = "Payload size", y = "Throughput (messages/sec)") +
    ggtitle("Throuput Distribution by Relay Count") +
    facet_wrap(. ~ relays)
}

save_plot <- function(plot, filename, height = 9, width = 16) {
  write(paste("-- generating", filename), stderr())
  ggsave(plot, filename = filename, height = height, width = width)
}

# -- main function -------------------------------------------------------------

read_data <- function(filename) {
  read.csv(filename) %>%
    mutate(relays = factor(relays), payload = factor(payload))
}

main <- function(args) {
  # Use a reasonable base font size.
  theme_set(theme_bw(base_size = 20))
  # Go through all files and plot the graphs.
  for (file in args) {
    data <- read_data(file)
    # Infer whether we're dealing with latency or throughput data.
    mode <- "unknown"
    if ("rtt" %in% colnames(data))
      mode <- "latency"
    else if ("messages" %in% colnames(data))
      mode <- "throughput"
    if (mode == "unknown")
       stop("invalid data columns: need 'relays,payload,(rtt|messages)'")
    if (mode == "latency") {
      # Plot 3 different graphs for latency because the y-axis changes by
      # several order of magnitudes. We don't want to use a log-linear plot
      # because it's more difficult to compare the various payload sizes
      # visually.
      data <- data %>%
        group_by(relays, payload) %>%
        summarize(rtt = median(rtt / 1e6))
      for (max_payload in 10^(6:8)) {
        filename <- paste(tools::file_path_sans_ext(file), max_payload, "pdf",
                          sep = ".")
        data %>%
          filter(as.numeric(levels(payload))[payload] <= max_payload) %>%
          plot_latency() %>%
          save_plot(filename)
      }
    } else if (mode == "throughput") {
      # Plot a single graph for throughput because larger payloads don't change
      # the picture.
      filename <- paste(tools::file_path_sans_ext(file), "pdf", sep = ".")
      data %>%
        group_by(relays, payload) %>%
        summarize(throughput = median(messages)) %>%
        plot_throughput() %>%
        save_plot(filename)
    }
  }
}

main(commandArgs(trailingOnly = TRUE))
