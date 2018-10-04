library(ggplot2)
w = 6
h = 4
data <- read.csv("latency.csv")
ggplot(data, aes(relays, rtt/1000000, group=relays)) + geom_boxplot() + facet_wrap(~ payload, labeller=label_both) + scale_y_continuous(trans='log10')
ggsave("latency_box.png", width=w, height=h, type="cairo")
ggplot(data, aes(relays, rtt, colour=payload)) + stat_ecdf()
ggsave("latency_ecdf.png", width=w, height=h, type="cairo")
