BEGIN { FS="," }
{ sub(/\r$/,"") }
function median(a,n, i,j,t) {
    for(i=1;i<=n;i++)for(j=i+1;j<=n;j++)if(a[j]<a[i]){t=a[i];a[i]=a[j];a[j]=t}
    return n%2?a[(n+1)/2]:(a[n/2]+a[n/2+1])/2
}
$1=="measured" {
    if($5+0<=0 || ($3!=1 && $3!=2)){bad=1;next}
    if($4=="v35"){
        if($2 in candidate)bad=1
        candidate[$2]=$5;a[++na]=$5;apos[$2]=$3
    }else if($4=="v34" || $4=="primesieve"){
        if($2 in control)bad=1
        if(reference!="" && reference!=$4)bad=1
        reference=$4;control[$2]=$5;b[++nb]=$5;bpos[$2]=$3
    }else bad=1
}
END {
    if(bad || na!=nb || na<2 || na%2){print "FAIL: incomplete paired comparison";exit 1}
    for(i=0;i<na;i++) {
        if(!(i in candidate) || !(i in control) || apos[i]!=(i%2?2:1) || bpos[i]!=(i%2?1:2)) {
            print "FAIL: missing pair or unexpected order";exit 1
        }
        ratios[i+1]=candidate[i]/control[i];if(candidate[i]<control[i])wins++
    }
    printf "MEDIANS v35=%.9f %s=%.9f seconds\n",median(a,na),reference,median(b,nb)
    ratio=median(ratios,na)
    printf "PAIRED v35_over_%s=%.9f time_reduction_pct=%+.6f v35_wins=%d/%d\n",reference,ratio,100*(1-ratio),wins,na
    print "Warmups excluded; all measured runs retained; separate processes and internal elapsed times."
    print "This is a configuration-specific experiment, not a universal speed or confidence-interval claim."
}
